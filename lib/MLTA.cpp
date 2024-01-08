//
// Created by prophe cheng on 2024/1/3.
//

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/IRBuilder.h"

#include "Common.h"
#include "MLTA.h"

#include <map>
#include <vector>


using namespace llvm;


//
// Implementation
//
pair<Type *, int> typeidx_c(Type *Ty, int Idx) {
    return make_pair(Ty, Idx);
}
pair<size_t, int> hashidx_c(size_t Hash, int Idx) {
    return make_pair(Hash, Idx);
}

bool MLTA::fuzzyTypeMatch(Type *Ty1, Type *Ty2,
                          Module *M1, Module *M2) {

    if (Ty1 == Ty2)
        return true;

    while (Ty1->isPointerTy() && Ty2->isPointerTy()) {
        Ty1 = Ty1->getPointerElementType();
        Ty2 = Ty2->getPointerElementType();
    }

    if (Ty1->isStructTy() && Ty2->isStructTy() &&
        (Ty1->getStructName().equals(Ty2->getStructName())))
        return true;
    if (Ty1->isIntegerTy() && Ty2->isIntegerTy() &&
        Ty1->getIntegerBitWidth() == Ty2->getIntegerBitWidth())
        return true;
    // TODO: more types to be supported.

    // Make the type analysis conservative: assume general
    // pointers, i.e., "void *" and "char *", are equivalent to
    // any pointer type and integer type.
    if (
            (Ty1 == Int8PtrTy[M1] &&
             (Ty2->isPointerTy() || Ty2 == IntPtrTy[M2]))
            ||
            (Ty2 == Int8PtrTy[M1] &&
             (Ty1->isPointerTy() || Ty1 == IntPtrTy[M2]))
            )
        return true;

    return false;
}

// FS = FS1 & FS2
void MLTA::intersectFuncSets(FuncSet &FS1, FuncSet &FS2,
                             FuncSet &FS) {
    FS.clear();
    for (auto F : FS1) {
        // 如果FS1中的F在FS2中
        if (FS2.find(F) != FS2.end())
            FS.insert(F);
    }
}


// Find targets of indirect calls based on function-type analysis: as
// long as the number and type of parameters of a function matches
// with the ones of the callsite, we say the function is a possible
// target of this call.
void MLTA::findCalleesWithType(CallInst *CI, FuncSet &S) {
    if (CI->isInlineAsm())
        return;
    //
    // Performance improvement: cache results for types
    //
    size_t CIH = callHash(CI);
    if (MatchedFuncsMap.find(CIH) != MatchedFuncsMap.end()) {
        if (!MatchedFuncsMap[CIH].empty())
            S.insert(MatchedFuncsMap[CIH].begin(),
                     MatchedFuncsMap[CIH].end());
        return;
    }

    CallBase *CB = dyn_cast<CallBase>(CI);
    for (Function *F : Ctx->AddressTakenFuncs) {
        // VarArg
        if (F->getFunctionType()->isVarArg()) {
            // Compare only known args in VarArg.
        }
            // otherwise, the numbers of args should be equal.
        else if (F->arg_size() != CB->arg_size()) {
            continue;
        }

        if (F->isIntrinsic()) {
            continue;
        }

        // Types completely match
        if (callHash(CI) == funcHash(F)) {
            S.insert(F);
            continue;
        }

        Module *CalleeM = F->getParent();
        Module *CallerM = CI->getFunction()->getParent();

        // Type matching on args.
        bool Matched = true;
        User::op_iterator AI = CB->arg_begin();
        for (Function::arg_iterator FI = F->arg_begin(), FE = F->arg_end(); FI != FE; ++FI, ++AI) {
            // Check type mis-matches.
            // Get defined type on callee side.
            Type *DefinedTy = FI->getType();
            // Get actual type on caller side.
            Type *ActualTy = (*AI)->getType();

            if (!fuzzyTypeMatch(DefinedTy, ActualTy, CalleeM, CallerM)) {
                Matched = false;
                break;
            }
        }

        // If args are matched, further check return types
        if (Matched) {
            Type *RTy1 = F->getReturnType();
            Type *RTy2 = CI->getType();
            if (!fuzzyTypeMatch(RTy1, RTy2, CalleeM, CallerM)) {
                Matched = false;
            }
        }

        if (Matched) {
            S.insert(F);
        }
    }
    MatchedFuncsMap[CIH] = S;
}

// 判断是不是复合类型
bool MLTA::isCompositeType(Type *Ty) {
    if (Ty->isStructTy()
        || Ty->isArrayTy()
        || Ty->isVectorTy())
        return true;
    else
        return false;
}

// 获取函数指针类型
Type *MLTA::getFuncPtrType(Value *V) {
    Type *Ty = V->getType();
    if (PointerType *PTy = dyn_cast<PointerType>(Ty)) {
        Type *ETy = PTy->getPointerElementType();
        if (ETy->isFunctionTy())
            return ETy;
    }

    return NULL;
}

// 获取value的base type
Value *MLTA::recoverBaseType(Value *V) {
    if (Instruction *I = dyn_cast<Instruction>(V)) {
        map<Value*, Value*> &AliasMap = AliasStructPtrMap[I->getFunction()];
        // 如果是int8*类型并且被cast到其它复杂数据指针类型
        if (AliasMap.find(V) != AliasMap.end())
            return AliasMap[V];
    }
    return NULL;
}

// 分析全局变量并收集function被分配给了哪些type，分析是field sensitive的
bool MLTA::typeConfineInInitializer(GlobalVariable *GV) {
    DBG << "Evaluation for global variable: " << GV->getName().str() << "\n";
    // 获取initializer信息
    Constant *Ini = GV->getInitializer();
    // 不是聚合常量，跳过。Aggregate Constants（聚合常量）是指由多个元素组成的常量值，如结构体（struct）或数组（array）。
    // 聚合常量是LLVM IR的一种表示形式，用于表示高级语言中的复合数据类型。
    if (!isa<ConstantAggregate>(Ini))
        return false;

    list<pair<Type*, int>> NestedInit;
    map<Value*, pair<Value*, int>> ContainersMap; // key为value->first的第value->second个子常量
    set<Value*> FuncOperands;
    list<User*> LU;
    set<Value*> Visited;
    LU.push_back(Ini);

    // BFS自顶向下访问聚合常量
    while (!LU.empty()) {
        User *U = LU.front();
        LU.pop_front();
        // 如果该聚合常量访问过，跳过
        if (Visited.find(U) != Visited.end()) {
            continue;
        }
        Visited.insert(U);
        // 获取聚合常量的类型
        Type *UTy = U->getType();
        assert(!UTy->isFunctionTy());
        // 如果是结构体类型的聚合常量，那么判定常量数量大于0
        if (StructType *STy = dyn_cast<StructType>(U->getType())) {
            if (U->getNumOperands() > 0)
                assert(STy->getNumElements() == U->getNumOperands());
            else
                continue;
        }
        // 遍历聚合常量中的每个子常量
        for (auto oi = U->op_begin(), oe = U->op_end(); oi != oe; ++oi) {
            Value *O = *oi;
            Type *OTy = O->getType(); // 该常量的类型
            // oi->getOperandNo为O在聚合常量中的索引，U为父常量
            // 表示O是U的第oi->getOperandNo个子常量
            ContainersMap[O] = make_pair(U, oi->getOperandNo());
            string subConstantText = getInstructionText(O);

            Function *FoundF = NULL; // 当前子常量下的Function Pointer变量
            // Case 1: function address is assigned to a type，如果当前子常量是函数指针
            if (Function *F = dyn_cast<Function>(O))
                FoundF = F; // 如果O是函数类型

            //  a composite-type object (value) is assigned to a
            // field of another composite-type object
            // 如果子常量O仍然是聚合常量，则加入worklist
            else if (isCompositeType(OTy)) {
                // confine composite types
                Type *ITy = U->getType();
                int ONo = oi->getOperandNo();

                // recognize nested composite types
                User *OU = dyn_cast<User>(O);
                LU.push_back(OU);
            }
            // case2: 该常量为pointer cast to int, 也就是将函数指针cast到intptr_t或者uintptr_t类型
            else if (PtrToIntOperator *PIO = dyn_cast<PtrToIntOperator>(O)) {
                // PIO->getOperand(0)返回ptrtoint的指针变量
                // 如果是函数指针
                Function *FF = dyn_cast<Function>(PIO->getOperand(0));
                if (FF)
                    FoundF = FF;
                // 有可能是case4，指向其它全局变量的指针
                else {
                    User *OU = dyn_cast<User>(PIO->getOperand(0)); // 如果指针指向聚合常量
                    LU.push_back(OU);
                }
            }
            // case3，将函数指针cast到void*或者char*类型
            else if (BitCastOperator *CO = dyn_cast<BitCastOperator>(O)) {
                // Virtual functions will always be cast by inserting the first parameter
                Function *CF = dyn_cast<Function>(CO->getOperand(0));
                if (CF) {
                    Type *ITy = U->getType();

                    // FIXME: Assume this is VTable
                    // 如果父常量不是结构体常量，那么很有可能是class，该function可能是virtual function
                    if (!ITy->isStructTy())
                        VTableFuncsMap[GV].insert(CF);

                    FoundF = CF;
                }
                else {
                    // 获取source操作数，有可能是case4，指向其它复杂数据类型全局变量的指针
                    User *OU = dyn_cast<User>(CO->getOperand(0));
                    LU.push_back(OU);
                }
            }
            // Case 3: a reference (i.e., pointer) of a composite-type
            // object is assigned to a field of another composite-type
            // object
            // 如果是指针类型
            else if (PointerType *POTy = dyn_cast<PointerType>(OTy)) {
                // 如果是NULL
                if (isa<ConstantPointerNull>(O))
                    continue;
                // if the pointer points a composite type, conservatively
                // treat it as a type cap (we cannot get the next-layer type
                // if the type is a cap)
                User *OU = dyn_cast<User>(O);
                LU.push_back(OU);
                // 如果指针指向全局变量，比如test4.c中 struct B b = { .a = &ba }; 这种
                if (GlobalVariable* GO = dyn_cast<GlobalVariable>(OU)) {
                    DBG << "subconstant: " << subConstantText << " point to global variable: "
                        << GO->getName().str() << "\n";
                    Type* Ty = POTy->getPointerElementType(); // 获取指针变量的类型
                    // FIXME: take it as a confinement instead of a cap
                    if (Ty->isStructTy())
                        typeCapSet.insert(typeHash(Ty));
                }
            }
            else {
                // TODO: Type escaping?
            }

            // Found a function
            if (FoundF && !FoundF->isIntrinsic()) {
                // "llvm.compiler.used" indicates that the linker may touch
                // it, so do not apply MLTA against them
                if (GV->getName() != "llvm.compiler.used")
                    StoredFuncs.insert(FoundF);

                // Add the function type to all containers
                Value* CV = O;
                set<Value *> Visited_; // to avoid loop
                while (ContainersMap.find(CV) != ContainersMap.end()) {
                    auto Container = ContainersMap[CV];

                    Type *CTy = Container.first->getType(); // 父聚合常量的类型
                    set<size_t> TyHS; // 所有满足当前层次对应的结构体type的hash
                    string type_name = getInstructionText(CTy);
                    if (StructType *STy = dyn_cast<StructType>(CTy))  // 如果父聚合常量是结构体
                        type_name = structTypeHash(STy, TyHS);
                    else
                        TyHS.insert(typeHash(CTy)); // 不是结构体类型

                    for (auto TyH : TyHS) {// 遍历所有可以和当前层次类型对应上的类型hash
                        typeIdxFuncsMap[TyH][Container.second].insert(FoundF);
                        DBG << Container.second << " field of Type: " << type_name <<
                        " add function: " << FoundF->getName().str() << "\n";
                    }

                    Visited_.insert(CV);
                    if (Visited_.find(Container.first) != Visited_.end())
                        break;

                    CV = Container.first;
                }
            }
        }
    }

    return true;
}

// This function precisely collect alias types for general pointers
// 收集Function F中的cast操作，主要收集char*, void*到其它复杂结构指针类型的cast，记录对应变量转换
void MLTA::collectAliasStructPtr(Function *F) {
    map<Value*, Value*> &AliasMap = AliasStructPtrMap[F];
    set<Value*> ToErase;
    for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
        Instruction *I = &*i;
        // 遍历所有的cast情况
        if (CastInst *CI = dyn_cast<CastInst>(I)) {
            Value *FromV = CI->getOperand(0);
            // TODO: we only consider calls for now
            if (!isa<CallInst>(FromV)) // FromTy是函数调用返回值
                continue;

            Type *FromTy = FromV->getType(); // 原始类型
            Type *ToTy = CI->getType(); // 转换后类型
            if (Int8PtrTy[F->getParent()] != FromTy) // FromTy必须为 int8* 类型，很多函数指针都会被cast到(int* )
                continue;
            // 转换后不是指针类型，跳过
            if (!ToTy->isPointerTy())
                continue;

            if (!isCompositeType(ToTy->getPointerElementType())) // 如果ToTy的基类型不是复杂类型
                continue;

            if (AliasMap.find(FromV) != AliasMap.end()) { // FromV已有其它ToV
                ToErase.insert(FromV);
                continue;
            }
            AliasMap[FromV] = CI;
        }
    }
    for (auto Erase : ToErase) // 不考虑多重重名的类型
        AliasMap.erase(Erase);
}


string getInstructionText(Value* inst) {
    std::string instructionText;
    raw_string_ostream stream(instructionText);
    inst->print(stream);
    stream.flush();
    return instructionText;
}

string getInstructionText(Type* type) {
    std::string instructionText;
    raw_string_ostream stream(instructionText);
    type->print(stream);
    stream.flush();
    return instructionText;
}

// 分析结构体field和address-taken function之间的约束
bool MLTA::typeConfineInFunction(Function *F) {
    DBG << "analyzing type confine in function: " << F->getName().str() << "\n";
    for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
        Instruction *I = &*i;

        // 如果是store
        if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
            Value *PO = SI->getPointerOperand();
            Value *VO = SI->getValueOperand();

            // 被store的是个function
            Function *CF = getBaseFunction(VO->stripPointerCasts());
            if (!CF)
                continue;
            if (F->isIntrinsic())
                continue;

            // 将指令的文本表示导入字符串
            string instructionText = getInstructionText(PO);
            DBG << "confining instruction: " << instructionText << " to function: " << CF->getName().str() << "\n";
            confineTargetFunction(PO, CF);
        }
        // 访问所有参数包含了函数指针的函数调用
        else if (CallInst *CI = dyn_cast<CallInst>(I)) {
            // 访问实参
            for (User::op_iterator OI = I->op_begin(), OE = I->op_end(); OI != OE; ++OI) {
                // 如果该参数为函数指针
                if (Function *FF = dyn_cast<Function>(*OI)) { // 如果该参数是个函数对象，即将函数指针作为参数
                    if (FF->isIntrinsic())
                        continue;
                    // 如果callsite是indirect-call
                    if (CI->isIndirectCall()) {
                        string instructionText = getInstructionText(*OI);
                        DBG << "confining instruction: " << instructionText
                            << " to function: " << FF->getName().str() << "\n";
                        confineTargetFunction(*OI, FF); // 将实参的类型和F绑定
                        continue;
                    }
                    // 如果不是间接调用，接着分析
                    Value *CV = CI->getCalledOperand();
                    Function *CF = dyn_cast<Function>(CV); // CF为被调用的函数
                    if (!CF)
                        continue;
                    // call target
                    if (CF->isDeclaration())
                        CF = Ctx->GlobalFuncMap[CF->getGUID()];
                    if (!CF)
                        continue;
                    // Arg为函数指针对应的形参
                    if (Argument *Arg = getParamByArgNo(CF, OI->getOperandNo())) { // CF为被调用的函数，这里返回函数指针对应的形参
                        // U为函数CF中使用了该形参的指令
                        // 遍历所有使用形参的指令
                        for (auto U : Arg->users()) {
                            // 如果使用形参的是store指令或者bitcast指令
                            if (isa<StoreInst>(U) || isa<BitCastOperator>(U)) {
                                // debug模式
                                // 将指令的文本表示导入字符串
                                string instructionText = getInstructionText(U);
                                DBG << "confining instruction: " << instructionText << " in function: " << CF->getName().str()
                                << " to function: " << FF->getName().str() << "\n";

                                confineTargetFunction(U, FF); // 指令U可能使用了指向F的函数指针
                            }
                        }
                    }
                    // TODO: track into the callee to avoid marking the
                    // function type as a cap
                }
            }
        }
    }

    return true;
}

// cast有3种情况：
// 1.store ptr value 2.memcpy(dst, src) 3.cast type1->type2
bool MLTA::typePropInFunction(Function *F) {
    // Two cases for propagation: store and cast.
    // For store, LLVM may use memcpy
    set<User*> CastSet;
    // 遍历F中所有的store指令
    for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
        Instruction *I = &*i;
        Value *PO = NULL, *VO = NULL;

        // case1: store
        if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
            PO = SI->getPointerOperand(); // store的指针变量，dest
            VO = SI->getValueOperand(); // 被store的value，也就是函数地址，source
        }
        // case2: 用聚合常量给结构体变量赋值
        else if (CallInst *CI = dyn_cast<CallInst>(I)) {
            Value *CV = CI->getCalledOperand();
            Function *CF = dyn_cast<Function>(CV); // called function
            if (CF) { // 如果是直接调用
                // LLVM may optimize struct assignment into a call to
                // intrinsic memcpy
                if (CF->getName() == "llvm.memcpy.p0i8.p0i8.i64") {
                    PO = CI->getOperand(0); // dest指向结构体变量
                    VO = CI->getOperand(1); // source指向聚合常量
                }
            }
        }

        if (PO && VO) {
            // TODO: if VO is a global with an initializer, this should be
            // taken as a confinement instead of propagation, which can
            // improve the precision
            // 不分析聚合常量以及普通常量数据
            if (isa<ConstantAggregate>(VO) || isa<ConstantData>(VO))
                continue;

            list<typeidx_t> TyList;
            Value* NextV = NULL;
            set<Value*> Visited;
            nextLayerBaseType(VO, TyList, NextV, Visited);
            if (!TyList.empty()) {
                for (auto TyIdx : TyList) {
                    DBG << "processing store instruction: " << getInstructionText(I) << "\n";
                    propagateType(PO, TyIdx.first, TyIdx.second);
                }
                continue;
            }

            Visited.clear();
            // source操作数基类型
            Type *BTy = getBaseType(VO, Visited);
            // Composite type
            if (BTy) {
                propagateType(PO, BTy);
                continue;
            }

            Type *FTy = getFuncPtrType(VO->stripPointerCasts());
            // Function-pointer type
            if (FTy) {
                if (!getBaseFunction(VO)) {
                    // 从FTy cast到PO
                    propagateType(PO, FTy);
                    continue;
                }
                else
                    continue;
            }
            // store中source出现了其它类型的指针
            if (!VO->getType()->isPointerTy())
                continue;
            // VO->getType()->isPointerTy()
            else
                // General-pointer type for escaping
                escapeType(PO);
        }

        // case3: Handle casts
        if (CastInst *CastI = dyn_cast<CastInst>(I))
            // Record the cast, handle later
            CastSet.insert(CastI);

        // Operands of instructions can be BitCastOperator
        for (User::op_iterator OI = I->op_begin(), OE = I->op_end();
             OI != OE; ++OI) {
            if (BitCastOperator *CO = dyn_cast<BitCastOperator>(*OI))
                CastSet.insert(CO);
        }
    }

    for (auto Cast : CastSet) {
        // TODO: we may not need to handle casts as casts are already
        // stripped out in confinement and propagation analysis. Also for
        // a function pointer to propagate, it is supposed to be stored
        // in memory.

        // The conservative escaping policy can be optimized，cast from struct type1 to struct type2
        Type *FromTy = Cast->getOperand(0)->getType();
        Type *ToTy = Cast->getType();
        if (FromTy->isPointerTy() && ToTy->isPointerTy()) {
            Type *EFromTy = FromTy->getPointerElementType();
            Type *EToTy = ToTy->getPointerElementType();
            if (EFromTy->isStructTy() && EToTy->isStructTy()) {
                DBG << "processing bitcast: " << getInstructionText(Cast) << "\n";
                propagateType(Cast, EFromTy, -1);
            }
        }
    }

    return true;
}


// 要么初始值是function，要么一直cast
Function *MLTA::getBaseFunction(Value *V) {
    if (Function *F = dyn_cast<Function>(V))
        if (!F->isIntrinsic())
            return F;

    Value *CV = V;

    // 函数指针可能被cast到其它类型的函数指针
    // 比如fptr_int f = (fptr_int)&f1对应的IR为 store void (i32)* bitcast (void (i64)* @f1 to void (i32)*), void (i32)** %f
    while (BitCastOperator *BCO = dyn_cast<BitCastOperator>(CV)) {
        Value *O = BCO->getOperand(0);
        if (Function *F = dyn_cast<Function>(O))
            if (!F->isIntrinsic())
                return F;
        CV = O;
    }
    return NULL;
}


// This function is to get the base type in the current layer.
// To get the type of next layer (with GEP and Load), use
// nextLayerBaseType() instead.
Type *MLTA::getBaseType(Value *V, set<Value*> &Visited) {
    if (!V)
        return NULL;

    if (Visited.find(V) != Visited.end())
        return NULL;
    Visited.insert(V);

    Type *Ty = V->getType();

    if (isCompositeType(Ty))
        return Ty;

    // The value itself is a pointer to a composite type
    else if (Ty->isPointerTy()) {
        Type *ETy = Ty->getPointerElementType();
        // 如果是复杂数据结构指针类型
        if (isCompositeType(ETy))
            return ETy;
        // 如果是被cast到其它类型的int8*类型，返回cast后的base类型
        else if (Value *BV = recoverBaseType(V))
            return BV->getType()->getPointerElementType();
    }

    if (BitCastOperator *BCO = dyn_cast<BitCastOperator>(V))
        return getBaseType(BCO->getOperand(0), Visited); // return source type

    else if (SelectInst *SelI = dyn_cast<SelectInst>(V))
        // Assuming both operands have same type, so pick the first
        // operand
        return getBaseType(SelI->getTrueValue(), Visited);

    else if (PHINode *PN = dyn_cast<PHINode>(V))
        // TODO: tracking incoming values
        return _getPhiBaseType(PN, Visited);

    else if (LoadInst *LI = dyn_cast<LoadInst>(V))
        return getBaseType(LI->getPointerOperand(), Visited);

    else if (Type *PTy = dyn_cast<PointerType>(Ty)) {
        // ??
    }
    else {
    }

    return NULL;
}


Type *MLTA::_getPhiBaseType(PHINode *PN, set<Value *> &Visited) {
    for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
        Value *IV = PN->getIncomingValue(i);

        Type *BTy = getBaseType(IV, Visited);
        if (BTy)
            return BTy;
    }

    return NULL;
}


// 假设var1.f1.f2 = (FromTy)var.
// 那么typeIdxPropMap[type(var1)][f1_idx], typeIdxPropMap[type(var1.f2)][f2_idx] add (FromTy, Idx)
void MLTA::propagateType(Value *ToV, Type *FromTy, int Idx) {
    list<typeidx_t> TyChain;
    bool Complete = true;
    getBaseTypeChain(TyChain, ToV, Complete); // 获取ToV的type chain
    DBG << "From type: " << getInstructionText(FromTy) << "\n";
    DBG << "To Value: " << getInstructionText(ToV) << "\n";
    for (auto T : TyChain) {
        // 如果type和From Type匹配
        if (typeHash(T.first) == typeHash(FromTy) && T.second == Idx)
            continue;

        typeIdxPropMap[typeHash(T.first)][T.second].insert(hashidx_c(typeHash(FromTy), Idx));
        DBG << "[PROP] " << *(FromTy) << ": " << Idx
            << "\n\t===> " << *(T.first) << " " << T.second << "\n";
    }
}

// Get the chain of base types for V
// Complete: whether the chain's end is not escaping---it won't
// propagate further
bool MLTA::getBaseTypeChain(list<typeidx_t> &Chain, Value *V, bool &Complete) {
    Complete = true;
    Value *CV = V, *NextV = NULL;
    list<typeidx_t> TyList;
    set<Value*> Visited;

    Type* BTy = getBaseType(V, Visited);
    if (BTy) {
        // 0 vs. -1?
        Chain.push_back(typeidx_c(BTy, 0));
    }
    Visited.clear();

    // 遍历每一层
    while (nextLayerBaseType(CV, TyList, NextV, Visited)) {
        CV = NextV;
    }
    for (auto TyIdx : TyList) {
        Chain.push_back(typeidx_c(TyIdx.first, TyIdx.second));
    }

    // Checking completeness
    if (!NextV)
        Complete = false;

    else if (isa<Argument>(NextV) && NextV->getType()->isPointerTy()) {
        Complete = false;
    }
    else {
        for (auto U : NextV->users()) {
            if (StoreInst *SI = dyn_cast<StoreInst>(U)) {
                if (NextV == SI->getPointerOperand()) {
                    Complete = false;
                    break;
                }
            }
        }
        // TODO: other cases like store?
    }

    if (!Chain.empty() && !Complete) {
        typeCapSet.insert(typeHash(Chain.back().first));
    }

    return true;
}


// Get the composite type of the lower layer. Layers are split by
// memory loads or GEP
bool MLTA::nextLayerBaseType(Value* V, list<typeidx_t> &TyList,
                             Value* &NextV, set<Value*> &Visited) {
    if (!V || isa<Argument>(V)) {
        NextV = V;
        return false;
    }

    if (Visited.find(V) != Visited.end()) {
        NextV = V;
        return false;
    }
    Visited.insert(V);

    // The only way to get the next layer type: GetElementPtrInst or
    // GEPOperator
    // gep格式为getelementptr inbounds %struct.ST, ptr %s, f1, f2, f3, f4, ...
    if (GEPOperator *GEP = dyn_cast<GEPOperator>(V)) {
        NextV = GEP->getPointerOperand(); // 返回s
        bool ret = getGEPLayerTypes(GEP, TyList);
        if (!ret)
            NextV = NULL;
        return ret;
    }
    // 如果是load指令
    else if (LoadInst *LI = dyn_cast<LoadInst>(V)) {
        NextV = LI->getPointerOperand();
        // 求基地址的结构体层次
        return nextLayerBaseType(LI->getOperand(0), TyList, NextV, Visited);
    }
    else if (BitCastOperator *BCO = dyn_cast<BitCastOperator>(V)) {
        NextV = BCO->getOperand(0); // 求source type的层次
        return nextLayerBaseType(BCO->getOperand(0), TyList, NextV, Visited);
    }
    // Phi and Select
    else if (PHINode *PN = dyn_cast<PHINode>(V)) {
        // FIXME: tracking incoming values
        bool ret = false;
        set<Value *> NVisited;
        list<typeidx_t> NTyList;
        for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
            Value *IV = PN->getIncomingValue(i);
            NextV = IV;
            NVisited = Visited;
            NTyList = TyList;
            ret = nextLayerBaseType(IV, NTyList, NextV, NVisited);
            if (NTyList.size() > TyList.size()) {
                break;
            }
        }
        TyList = NTyList;
        Visited = NVisited;
        return ret;
    }
    else if (SelectInst *SelI = dyn_cast<SelectInst>(V)) {
        // Assuming both operands have same type, so pick the first
        // operand
        NextV = SelI->getTrueValue();
        return nextLayerBaseType(SelI->getTrueValue(), TyList, NextV, Visited);
    }
    // Other unary instructions
    // FIXME: may introduce false positives
    else if (UnaryOperator *UO = dyn_cast<UnaryOperator>(V)) {
        NextV = UO->getOperand(0);
        return nextLayerBaseType(UO->getOperand(0), TyList, NextV, Visited);
    }

    NextV = NULL;
    return false;
}


void MLTA::escapeType(Value *V) {
    list<typeidx_t> TyChain;
    bool Complete = true;
    getBaseTypeChain(TyChain, V, Complete);
    for (auto T : TyChain) {
        DBG<<"[Escape] Type: " << *(T.first)<< "; Idx: " <<T.second<< "\n";
        typeEscapeSet.insert(typeIdxHash(T.first, T.second));
    }
}


void MLTA::confineTargetFunction(Value *V, Function *F) {
    if (F->isIntrinsic())
        return;

    StoredFuncs.insert(F);

    list<typeidx_t> TyChain;
    bool Complete = true;
    // 获取变量V的类型层次
    getBaseTypeChain(TyChain, V, Complete);
    for (auto TI : TyChain) {
        DBG << TI.second << " field of Type: " << getInstructionText(TI.first) <<
            " add function: " << F->getName().str() << "\n";
        typeIdxFuncsMap[typeHash(TI.first)][TI.second].insert(F);
    }
    if (!Complete) {
        if (!TyChain.empty())
            typeCapSet.insert(typeHash(TyChain.back().first));
        else
            typeCapSet.insert(funcHash(F));
    }
}



bool MLTA::getGEPLayerTypes(GEPOperator *GEP, list<typeidx_t> &TyList) {
    Value* PO = GEP->getPointerOperand(); // base结构体变量
    Type* ETy = GEP->getSourceElementType(); // 通常是base结构体类型

    vector<int> Indices;
    list<typeidx_t> TmpTyList;
    // FIXME: handle downcasting: the GEP may get a field outside the base type Or use O0 to avoid this issue
    ConstantInt *ConstI = dyn_cast<ConstantInt>(GEP->idx_begin()->get());
    // 遍历getelementptr指令所有的索引
    if (ConstI && ConstI->getSExtValue() != 0) {
        // FIXME: The following is an attempt to handle the intentional
        // out-of-bound access; however, it is not fully working, so I
        // skip it for now
        //
        Instruction *I = dyn_cast<Instruction>(PO);
        Value *BV = recoverBaseType(PO);
        // 如果是int8*指针cast到其它指针
        if (BV) {
            // 获取被cast到的指针类型
            ETy = BV->getType()->getPointerElementType();
            APInt Offset (ConstI->getBitWidth(),
                          ConstI->getZExtValue());
            Type *BaseTy = ETy;
            SmallVector<APInt>IndiceV = DLMap[I->getModule()]->getGEPIndicesForOffset(BaseTy, Offset);
            for (auto Idx : IndiceV) {
                Indices.push_back(*Idx.getRawData());
            }
        }
        else if (StructType *STy = dyn_cast<StructType>(ETy)) {
            bool OptGEP = false;
            for (auto User : GEP->users()) {
                if (BitCastOperator *BCO = dyn_cast<BitCastOperator>(User)) {
                    OptGEP = true;
#ifdef SOUND_MODE
                    // TODO: This conservative decision results may cases
					// disqualifying MLTA. Need an analysis to recover the base
					// types, or use O0 to avoid the optimization
					return false;
#endif
                }
            }
        }
    }

    if (Indices.empty()) {
        for (auto it = GEP->idx_begin(); it != GEP->idx_end(); it++) {
            ConstantInt *ConstII = dyn_cast<ConstantInt>(it->get());
            if (ConstII)
                Indices.push_back(ConstII->getSExtValue());
            else
                Indices.push_back(-1);
        }
    }

    // 遍历结构体层次
    for (auto it = Indices.begin() + 1; it != Indices.end(); it++) {
        int Idx = *it;
        TmpTyList.push_front(typeidx_c(ETy, Idx));

        // Continue to parse subty
        Type* SubTy = NULL;
        if (StructType *STy = dyn_cast<StructType>(ETy)) {
            SubTy = STy->getElementType(Idx);
        }
        else if (ArrayType *ATy = dyn_cast<ArrayType>(ETy)) {
            SubTy = ATy->getElementType();
        }
        else if (VectorType *VTy = dyn_cast<VectorType>(ETy)) {
            SubTy = VTy->getElementType();
        }
        assert(SubTy);

        ETy = SubTy;
    }
    // This is a trouble caused by compiler optimization that
    // eliminates the access path when the index of a field is 0.
    // Conservatively assume a base-struct pointer can serve as a
    // pointer to its first field
    StructType *STy = dyn_cast<StructType>(ETy);
    if (STy && STy->getNumElements() > 0) {
        // Get the type of its first field
        Type *Ty0 = STy->getElementType(0);
        for (auto U : GEP->users()) {
            if (BitCastOperator *BCO = dyn_cast<BitCastOperator>(U)) {
                if (PointerType *PTy = dyn_cast<PointerType>(BCO->getType())) {
                    Type *ToTy = PTy->getPointerElementType();
                    if (Ty0 == ToTy)
                        TmpTyList.push_front(typeidx_c(ETy, 0));
                }
            }
        }
    }

    if (!TmpTyList.empty()) {
        // Reorder
        for (auto TyIdx : TmpTyList) {
            TyList.push_back(TyIdx);
        }
        return true;
    }
    else
        return false;
}


// indirect-call求解部分
// The API for MLTA: it returns functions for an indirect call
bool MLTA::findCalleesWithMLTA(CallInst *CI, FuncSet &FS) {
    // Initial set: first-layer results
    // TODO: handling virtual functions
    // 获得FLTA结果
    FS = Ctx->sigFuncsMap[callHash(CI)];

    if (FS.empty())
        // No need to go through MLTA if the first layer is empty
        return false;

    FuncSet FS1, FS2;
    Type *PrevLayerTy = (dyn_cast<CallBase>(CI))->getFunctionType();
    int PrevIdx = -1;
    // callee expression
    Value *CV = CI->getCalledOperand();
    Value *NextV = NULL;
    int LayerNo = 1;

    // Get the next-layer type
    list<typeidx_t> TyList;
    bool ContinueNextLayer = true;
    while (ContinueNextLayer) {
        // Check conditions
        if (LayerNo >= MAX_TYPE_LAYER)
            break;

#ifdef SOUND_MODE
        // isNotSupported(CurType)
        if (typeCapSet.find(typeHash(PrevLayerTy)) != typeCapSet.end())
			break;
#endif

        set<Value*> Visited;
        nextLayerBaseType(CV, TyList, NextV, Visited);
        if (TyList.empty())
            break;

        for (typeidx_t TyIdx : TyList) {
            if (LayerNo >= MAX_TYPE_LAYER)
                break;
            ++LayerNo;

            size_t TyIdxHash = typeIdxHash(TyIdx.first, TyIdx.second);
            // -1 represents all possible fields of a struct
            size_t TyIdxHash_1 = typeIdxHash(TyIdx.first, -1);

            // Caching for performance
            if (MatchedFuncsMap.find(TyIdxHash) != MatchedFuncsMap.end())
                FS1 = MatchedFuncsMap[TyIdxHash];
            else {
#ifdef SOUND_MODE
                // CurType ∈ escaped-type
                if (typeEscapeSet.find(TyIdxHash) != typeEscapeSet.end())
					break;
				if (typeEscapeSet.find(TyIdxHash_1) != typeEscapeSet.end())
					break;
#endif
                getTargetsWithLayerType(typeHash(TyIdx.first), TyIdx.second, FS1);
                // Collect targets from dependent types that may propagate
                // targets to it
                set<hashidx_t> PropSet;
                getDependentTypes(TyIdx.first, TyIdx.second, PropSet);
                // for each PropType in type-propagation[CurType] do
                for (auto Prop : PropSet) {
                    // 如果存在fromTypeIdx --> curTypeIDx
                    getTargetsWithLayerType(Prop.first, Prop.second, FS2);
                    FS1.insert(FS2.begin(), FS2.end());
                }
                MatchedFuncsMap[TyIdxHash] = FS1;
            }

            // Next layer may not always have a subset of the previous layer
            // because of casting, so let's do intersection
            intersectFuncSets(FS1, FS, FS2); // FS2 = FS & FS1
            FS = FS2; // FS = FS & FS1
            CV = NextV;

#ifdef SOUND_MODE
            if (typeCapSet.find(typeHash(TyIdx.first)) != typeCapSet.end()) {
				ContinueNextLayer = false;
				break;
			}
#endif

            PrevLayerTy = TyIdx.first;
            PrevIdx = TyIdx.second;
        }
        TyList.clear();
    }

    if (LayerNo > 1) {
        Ctx->NumSecondLayerTypeCalls++;
        Ctx->NumSecondLayerTargets += FS.size();
    }
    else {
        Ctx->NumFirstLayerTargets += Ctx->sigFuncsMap[callHash(CI)].size();
        Ctx->NumFirstLayerTypeCalls += 1;
    }

    return true;
}


bool MLTA::getDependentTypes(Type* Ty, int Idx, set<hashidx_t> &PropSet) {
    list<hashidx_t> LT;
    LT.push_back(hashidx_c(typeHash(Ty), Idx));
    set<hashidx_t> Visited;

    while (!LT.empty()) {
        hashidx_t TI = LT.front();
        LT.pop_front();
        if (Visited.find(TI) != Visited.end()) {
            continue;
        }
        Visited.insert(TI);

        for (hashidx_t Prop : typeIdxPropMap[TI.first][TI.second]) {
            PropSet.insert(Prop);
            LT.push_back(Prop);
        }
        for (hashidx_t Prop : typeIdxPropMap[TI.first][-1]) {
            PropSet.insert(Prop);
            LT.push_back(Prop);
        }
    }
    return true;
}


// Get all possible targets of the given type
bool MLTA::getTargetsWithLayerType(size_t TyHash, int Idx,
                                   FuncSet &FS) {
    // Get the direct funcset in the current layer, which
    // will be further unioned with other targets from type
    // casting
    if (Idx == -1) {
        for (const auto& FSet : typeIdxFuncsMap[TyHash])
            FS.insert(FSet.second.begin(), FSet.second.end());
    }
    else {
        FS = typeIdxFuncsMap[TyHash][Idx];
        FS.insert(typeIdxFuncsMap[TyHash][-1].begin(), typeIdxFuncsMap[TyHash][-1].end());
    }

    return true;
}