//
// Created by prophe cheng on 2025/4/6.
//
#include "llvm/IR/InstIterator.h"
#include "Passes/KELPPass.h"

// 与FLTA和MLTA不同，Kelp这里暂时不把confined address taken function添加金Ctx->AddressTakenFuncs
bool KELPPass::doInitialization(Module* M) {
    OP << "#" << MIdx << " Initializing: " <<M->getName()<<"\n";
    ++MIdx;
    CallGraphPass::doInitialization(M);
    DLMap[M] = &(M->getDataLayout());
    Int8PtrTy[M] = Type::getInt8PtrTy(M->getContext()); // int8 type id: 15
    IntPtrTy[M] = DLMap[M]->getIntPtrType(M->getContext()); // int type id: 13

    set<Function*> potentialConfFuncs;
    set<Value*> totalDefUseSites; // DefUseReachingSites in paper

    OP << "resolving confined global variables\n";
    for (Module::global_iterator gi = M->global_begin(); gi != M->global_end(); ++gi) {
        GlobalVariable* GV = &*gi;
        if (!(GV->getType()->isPointerTy() && GV->getType()->getNonOpaquePointerElementType()->isPointerTy() &&
            GV->getType()->getNonOpaquePointerElementType()->getNonOpaquePointerElementType()->isFunctionTy()))
            continue;

        set<Value*> globDefUseSites;
        set<Function*> referedFuncs;
        if (GV->hasInitializer()) {
            if (Function* RF = dyn_cast<Function>(GV->getInitializer())) {
                referedFuncs.insert(RF);
                globDefUseSites.insert(GV->getInitializer());
            }
        }

        // check all users of the global variable. It should not propagate to memory object.
        bool flag = true;
        for (User* U: GV->users()) {
            // store inst: *g = func;
            if (StoreInst* SGI = dyn_cast<StoreInst>(U)) {
                Function* SF = dyn_cast<Function>(SGI->getValueOperand());
                if (SF) {
                    globDefUseSites.insert(SGI);
                    referedFuncs.insert(SF);
                }
            }

            // v = *g;
            else if (LoadInst* LGI = dyn_cast<LoadInst>(U)) {
                if (!forwardAnalyze(LGI)) {
                    flag = false;
                    break;
                }
            }

            else {
                flag = false;
                break;
            }
        }
        // confined global variables
        if (flag) {
            confinedGlobs2Funcs[GV].insert(referedFuncs.begin(), referedFuncs.end());
            totalDefUseSites.insert(globDefUseSites.begin(), globDefUseSites.end());
        }
    }

    OP << "resolving simple function pointer start\n";
    // resolve simple indirect calls and their targets
    for (Function &F: *M) {
        if (F.isDeclaration())
            continue;
        for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
            Instruction *I = &*i;
            if (CallInst* CI = dyn_cast<CallInst>(I)) {
                if (!CI->isIndirectCall())
                    continue;
                set<Function*> callees;
                set<Value*> defUseSites;
                set<Function*> visitedFuncs;

                // if refered to global variables
                if (GlobalVariable* GV = dyn_cast<GlobalVariable>(CI->getCalledOperand())) {
                    if (confinedGlobs2Funcs.count(GV)) {
                        simpleIndCalls.insert(CI);
                        Ctx->Callees[CI].insert(confinedGlobs2Funcs[GV].begin(), confinedGlobs2Funcs[GV].end());
                        potentialConfFuncs.insert(callees.begin(), callees.end());
                        continue;
                    }
                }

                bool flag = resolveSFP(CI, CI->getCalledOperand(), callees, defUseSites, visitedFuncs);
                // simple indirect call
                if (flag) {
                    simpleIndCalls.insert(CI);
                    Ctx->Callees[CI].insert(callees.begin(), callees.end());
                    // mark those function as potential confined functions
                    potentialConfFuncs.insert(callees.begin(), callees.end());
                    totalDefUseSites.insert(defUseSites.begin(), defUseSites.end());
                }
            }
        }

        // resolve confined function with pure sys API call.
        if (!F.hasAddressTaken() || potentialConfFuncs.count(&F))
            continue;
        bool flag = true;
        // whether F is confined
        for (User* U : F.users()) {
            if (CallInst* CI = dyn_cast<CallInst>(U)) {
                if (!CI->isIndirectCall() && CI->getCalledFunction()) {
                    // systemCall(...,f,...)
                    if (CI->getCalledFunction()->isDeclaration() &&
                        sysAPIs.count(CI->getCalledFunction()->getName().str()) &&
                        &F == CI->getArgOperand(sysAPIs[CI->getCalledFunction()->getName().str()]))
                        continue;
                    // f(...)
                    else if (CI->getCalledFunction() == &F)
                        continue;
                }

                flag = false;
            }
            else
                flag = false;
            break;
        }
        // all system API usage
        if (flag)
            confinedAddrTakenFuncs.insert(&F);
    }

    OP << "resolving simple function pointer end\n";

    // resolve confined functions
    for (Function* PCF: potentialConfFuncs) {
        bool flag = true;
        for (User* U : PCF->users()) {
            if (CallInst* CI = dyn_cast<CallInst>(U)) {
                // direct call PCF, continue
                if (CI->getCalledOperand() == PCF)
                    continue;
            }
            if (!totalDefUseSites.count(U)) {
                flag = false;
                break;
            }
        }

        if (flag)
            confinedAddrTakenFuncs.insert(PCF);
    }

    OP << "resolving confined functions end\n";

    Ctx->NumSimpleIndCalls += simpleIndCalls.size();
    Ctx->NumConfinedFuncs += confinedAddrTakenFuncs.size();

    // MLTA initialization
    set<User*> CastSet;
    // ToDo: 遍历module下所有的结构体，处理别名结构体关系
    // ToDo: 比如%struct.ngx_http_upstream_peer_t.4391和%struct.ngx_http_upstream_peer_t
    // 处理同名结构体
    // Iterate and process globals，处理该module内的全局变量
    for (Module::global_iterator gi = M->global_begin(); gi != M->global_end(); ++gi) {
        GlobalVariable* GV = &*gi;

        // 如果该全局变量有初始化操作
        if (GV->hasInitializer()) {
            Type *ITy = GV->getInitializer()->getType();
            if (!ITy->isPointerTy() && !isCompositeType(ITy)) // 如果不是指针类型或者复杂数据类型，跳过
                continue;

            // 保存该全局变量
            Ctx->Globals[GV->getGUID()] = GV;
            // 解析全局变量的initializer
            typeConfineInInitializer(GV);
        }
    }

    OP << "confine global variable done\n";

    // Iterate functions and instructions
    for (Function &F : *M) {
        // Collect address-taken functions.
        // NOTE: declaration functions can also have address taken
        if (F.hasAddressTaken() && !isVirtualFunction(&F) && !confinedAddrTakenFuncs.count(&F)) {
            Ctx->AddressTakenFuncs.insert(&F);
            size_t FuncHash = Ctx->util.funcHash(&F, false);
            // 添加FLTA的结果
            // function的hash，用来进行FLTA，后面可能会修改
            Ctx->sigFuncsMap[FuncHash].insert(&F);
        }

        // The following only considers actual functions with body
        if (F.isDeclaration())
            continue;

        // 计算类型别名信息
        collectAliasStructPtr(&F);
        // 计算结构体field和function之间的约束
        typeConfineInFunction(&F);
        // 类型传播
        typePropInFunction(&F);

        // Collect global function definitions.
        if (F.hasExternalLinkage())
            Ctx->GlobalFuncMap[F.getGUID()] = &F;
    }

    // 处理外部链接的函数
    if (Ctx->Modules.size() == MIdx) {
        // Map the declaration functions to actual ones
        // NOTE: to delete an item, must iterate by reference
        for (auto &SF : Ctx->sigFuncsMap) {
            // 遍历所有的external link function
            for (auto F : SF.second) {
                if (!F)
                    continue;
                // 保留外部链接的函数
                if (F->isDeclaration()) {
                    SF.second.erase(F);
                    if (Function *AF = Ctx->GlobalFuncMap[F->getGUID()])
                        SF.second.insert(AF);
                }
            }
        }

        for (auto &TF: typeIdxFuncsMap) {
            for (auto &IF : TF.second) {
                for (auto F : IF.second) {
                    if (F->isDeclaration()) {
                        IF.second.erase(F);
                        if (Function *AF = Ctx->GlobalFuncMap[F->getGUID()])
                            IF.second.insert(AF);
                    }
                }
            }
        }
    }


    return false;
}

// add confined functions to address-taken function set
bool KELPPass::doFinalization(Module *M) {
    Ctx->AddressTakenFuncs.insert(confinedAddrTakenFuncs.begin(), confinedAddrTakenFuncs.end());
    CallGraphPass::doFinalization(M);
    return false;
}

// resolve complex indirect calls
void KELPPass::analyzeIndCall(CallInst* CI, FuncSet* FS) {
    if (simpleIndCalls.count(CI)) {
        FS->insert(Ctx->Callees[CI].begin(), Ctx->Callees[CI].end());
        return;
    }

    MLTADFPass::analyzeIndCall(CI, FS);
}

// should not propagate to memory object, if used in Store or Other inst,
// this is like MLTADFPass::justifyUser
bool KELPPass::forwardAnalyze(Value* V) {
    for (User* user: V->users()) {
        if (user == V)
            continue;
        // if function pointer: f is used in CallInst, it is either call: f(xxx) or pass arguments f1(f,...).
        else if (CallInst* CI = dyn_cast<CallInst>(user)) {
            if (CI->isIndirectCall()) {
                // if f is the called operand, ok
                if (CI->getCalledOperand() == V)
                    continue;
                // f is an argument of indirect-call, we don't know so we conservatively mark it as no
                else
                    return false;
            }
            else {
                // used in direct call: func(.., f, ..),
                Function* callee = CI->getCalledFunction();
                unsigned idx = -1;
                for (idx = 0; idx < CI->getNumOperands() - 1; ++idx)
                    if (CI->getOperand(idx) == V)
                        break;

                if (idx != CI->getNumOperands() - 1) {
                    bool flag = forwardAnalyze(callee->getArg(idx));
                    if (!flag)
                        return false;
                }
            }
        }
        // if function pointer: f is used in direct-value flow
        else if (isa<CmpInst>(user) || isa<PHINode>(user) ||
                isa<BitCastInst>(user) || isa<PtrToIntInst>(user) || isa<IntToPtrInst>(user) ||
                isa<BitCastOperator>(user) || isa<PtrToIntOperator>(user)) {
            bool flag = forwardAnalyze(dyn_cast<Value>(user));
            if (!flag)
                return false;
        }
        // function pointer f is not allowed to data flow to other values
        else
            return false;
    }
    return true;
}

bool KELPPass::resolveSFP(Value* User, Value* V, set<Function*>& callees, set<Value*>& defUseSites,
                set<Function*>& visitedFuncs) {
    if (!V)
        return true;

    if (Function* CF = dyn_cast<Function>(V)) {
        callees.insert(CF);
        defUseSites.insert(User);
        return true;
    }

    if (!justifyUsers(V, User))
        return false;

    // The type of Copy coule be: bitcast, ptrtoint, inttoptr
    if (isa<BitCastInst>(V) || isa<PtrToIntInst>(V) || isa<IntToPtrInst>(V)) {
        Instruction* VI = dyn_cast<Instruction>(V);
        return resolveSFP(VI, VI->getOperand(0), callees, defUseSites, visitedFuncs);
    }
        // nested cast could appear in instructions:
        // for example: %fadd.1 = phi i64 (i32, i32)* [ bitcast (i32 (i32, i32)* @add1 to i64 (i32, i32)*), %if.then ], [ %fadd.0, %if.end ]
    else if (isa<BitCastOperator>(V) || isa<PtrToIntOperator>(V)) {
        Operator* O = dyn_cast<Operator>(V);
        return resolveSFP(O, O->getOperand(0), callees, defUseSites, visitedFuncs);
    }

    else if (PHINode* PN = dyn_cast<PHINode>(V)) {
        for (unsigned i = 0; i < PN->getNumIncomingValues(); ++i) {
            Value* IV = PN->getIncomingValue(i);
            bool flag = resolveSFP(PN, IV, callees, defUseSites, visitedFuncs);
            if (!flag)
                return false;
        }
        return true;
    }

    else if (Argument* arg = dyn_cast<Argument>(V)) {
        // if current function is address-taken-function,
        // which means the function pointer may be passed throught indirect-call.
        // Hence its hard to find all targets, we conservatively deem it as non-simple indirect call.
        Function* F = arg->getParent();
        visitedFuncs.insert(F);
        if (F->hasAddressTaken())
            return false;
        unsigned argIdx;
        for (argIdx = 0; argIdx < F->arg_size(); ++argIdx)
            if (F->getArg(argIdx) == arg)
                break;
        if (argIdx == F->arg_size())
            return false;

        // Note: recursive call
        for (CallInst* Caller: Ctx->Callers[F]) {
            Function* PF = Caller->getFunction();
            if (visitedFuncs.count(PF))
                continue;
            if (!resolveSFP(Caller, Caller->getArgOperand(argIdx), callees, defUseSites, visitedFuncs))
                return false;
        }
        return true;
    }

        // function pointer: f = getF(...), where getF return function pointer.
    else if (CallInst* CI = dyn_cast<CallInst>(V)) {
        // if function pointer is retrived by indirect call, we conservatively deem it as non-simple function pointer
        if (CI->isIndirectCall())
            return false;
        // handle recursive call
        Function* curF = CI->getFunction();
        visitedFuncs.insert(curF);
        Function* CF = dyn_cast<Function>(CI->getCalledOperand());
        if (!CF)
            return false;
        if (CF->isDeclaration())
            return false;
        if (visitedFuncs.count(CF))
            return true;
        // back traverse from every return inst of CF
        for (inst_iterator i = inst_begin(CF), e = inst_end(CF); i != e; ++i) {
            Instruction* I = &*i;
            if (ReturnInst* RI = dyn_cast<ReturnInst>(I)) {
                if (!resolveSFP(RI, RI->getReturnValue(), callees, defUseSites, visitedFuncs))
                    return false;
            }
        }
        return true;
    }

    // load from confined global variable is allowed
    else if (LoadInst* LI = dyn_cast<LoadInst>(V)) {
        if (GlobalVariable* GV = dyn_cast<GlobalVariable>(LI->getPointerOperand())) {
            if (confinedGlobs2Funcs.count(GV)) {
                callees.insert(confinedGlobs2Funcs[GV].begin(), confinedGlobs2Funcs[GV].end());
                return true;
            }
            return false;
        }
        return false;
    }
    // encounter instruction such as: load, store.
    // Conservatively deem it as non-simple indirect call
    else
        return false;
}
