//
// Created by prophe cheng on 2024/1/3.
//

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/IRBuilder.h"

#include "Common.h"
#include "CallGraph.h"

#include <vector>


using namespace llvm;

//
// Implementation
//
void CallGraphPass::doMLTA(Function *F) {
    unrollLoops(F);
    // Collect callers and callees
    for (inst_iterator i = inst_begin(F), e = inst_end(F);
         i != e; ++i) {
        // Map callsite to possible callees.
        if (CallInst *CI = dyn_cast<CallInst>(&*i)) {
            CallSet.insert(CI);

            FuncSet *FS = &Ctx->Callees[CI];
            Value *CV = CI->getCalledOperand();
            Function *CF = dyn_cast<Function>(CV);

            // Indirect call
            if (CI->isIndirectCall()) {
                // Multi-layer type matching
                if (ENABLE_MLTA)
                    findCalleesWithMLTA(CI, *FS);
                else {
                    // 如果是签名匹配
                    if (ENABLE_SIGMATCH)
                        *FS = Ctx->sigFuncsMap[callHash(CI)];
                    // 如果是参数数量匹配
                    else {
                        size_t CIH = callHash(CI);
                        if (MatchedICallTypeMap.find(CIH) != MatchedICallTypeMap.end())
                            *FS = MatchedICallTypeMap[CIH];
                        else {
                            findCalleesWithType(CI, *FS);
                            MatchedICallTypeMap[CIH] = *FS;
                        }
                    }
                }


                for (Function *Callee : *FS)
                    // OP << "**** solving callee: " << Callee->getName().str() << "\n";
                    Ctx->Callers[Callee].insert(CI);

                // Save called values for future uses.
                Ctx->IndirectCallInsts.push_back(CI);

                ICallSet.insert(CI);
                if (!FS->empty()) {
                    MatchedICallSet.insert(CI);
                    Ctx->NumIndirectCallTargets += FS->size();
                    Ctx->NumValidIndirectCalls++;
                }
            }
            // Direct call
            else {
                // not InlineAsm
                if (CF) {
                    // Call external functions
                    if (CF->isDeclaration()) {
                        if (Function *GF = Ctx->GlobalFuncMap[CF->getGUID()])
                            CF = GF;
                    }

                    FS->insert(CF);
                    Ctx->Callers[CF].insert(CI);
                }
                // InlineAsm
                else {
                    // TODO: handle InlineAsm functions
                }
            }
        }
    }
}

// Layered Type Analysis
bool CallGraphPass::doInitialization(Module *M) {
    OP<<"#"<<MIdx<<" Initializing: "<<M->getName()<<"\n";

    ++ MIdx;

    DLMap[M] = &(M->getDataLayout());
    Int8PtrTy[M] = Type::getInt8PtrTy(M->getContext()); // int8 type id: 15
    IntPtrTy[M] = DLMap[M]->getIntPtrType(M->getContext()); // int type id: 13

    set<User *>CastSet;

    //
    // Iterate and process globals，处理该module内的全局变量
    //
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

    // Iterate functions and instructions
    for (Function &F : *M) {
        // Collect address-taken functions.
        // NOTE: declaration functions can also have address taken
        if (F.hasAddressTaken()) {
            Ctx->AddressTakenFuncs.insert(&F);
            size_t FuncHash = funcHash(&F, false);
            // 添加FLTA的结果
            // function的hash，用来进行FLTA，后面可能会修改
            Ctx->sigFuncsMap[FuncHash].insert(&F);
            StringRef FName = F.getName();
            if (FName.startswith("__x64") ||
                FName.startswith("__ia32")) {
                OutScopeFuncs.insert(&F);
            }
        }

        // The following only considers actual functions with body
        if (F.isDeclaration()) {
            continue;
        }

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

        MIdx = 0;
    }

    return false;
}

bool CallGraphPass::doFinalization(Module *M) {
    ++MIdx;
    if (Ctx->Modules.size() == MIdx) {
        // Finally map declaration functions to actual functions
        OP<<"Mapping declaration functions to actual ones...\n";
        Ctx->NumIndirectCallTargets = 0;
        for (auto CI : CallSet) {
            FuncSet FS;
            for (auto F : Ctx->Callees[CI]) {
                if (F->isDeclaration()) {
                    F = Ctx->GlobalFuncMap[F->getGUID()];
                    if (F) {
                        FS.insert(F);
                    }
                }
                else
                    FS.insert(F);
            }
            Ctx->Callees[CI] = FS;

            if (CI->isIndirectCall())
                Ctx->NumIndirectCallTargets += FS.size();
        }

    }
    return false;
}

bool CallGraphPass::doModulePass(Module *M) {
    ++ MIdx;
    //
    // Iterate and process globals
    //
    for (Module::global_iterator gi = M->global_begin();
         gi != M->global_end(); ++gi) {
        GlobalVariable* GV = &*gi;

        Type *GTy = GV->getType();
        assert(GTy->isPointerTy());

    }
    if (MIdx == Ctx->Modules.size()) {
    }
    //
    // Process functions
    //
    for (Module::iterator f = M->begin(), fe = M->end();f != fe; ++f) {
        Function *F = &*f;

        if (F->isDeclaration())
            continue;

        doMLTA(F);
    }

    return false;
}