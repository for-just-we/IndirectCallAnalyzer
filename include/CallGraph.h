//
// Created by prophe cheng on 2024/1/3.
//

#ifndef TYPEDIVE_CALLGRAPH_H
#define TYPEDIVE_CALLGRAPH_H

#include "Analyzer.h"
#include "MLTA.h"
#include "Config.h"

// 输入LLVM Module，运行call graph分析
class CallGraphPass : public virtual IterativeModulePass, public virtual MLTA {
private:
    //
    // Variables
    //
    // Index of the module
    int MIdx;

    set<CallInst*> CallSet;
    set<CallInst*> ICallSet;
    set<CallInst*> MatchedICallSet;

    //
    // Methods
    //
    void doMLTA(Function *F);

public:
    static int AnalysisPhase;

    CallGraphPass(GlobalContext *Ctx_)
            : IterativeModulePass(Ctx_, "CallGraph"),
              MLTA(Ctx_) {

        Ctx->util.LoadElementsStructNameMap(Ctx->Modules);
        MIdx = 0;
    }

    virtual bool doInitialization(llvm::Module *);

    virtual bool doFinalization(llvm::Module *);

    virtual bool doModulePass(llvm::Module *);

    virtual void processAliasStruct(llvm::Module *);
};


#endif //TYPEDIVE_CALLGRAPH_H
