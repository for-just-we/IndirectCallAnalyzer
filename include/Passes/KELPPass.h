//
// Created by prophe cheng on 2025/4/4.
//

#ifndef INDIRECTCALLANALYZER_KELPPASS_H
#define INDIRECTCALLANALYZER_KELPPASS_H

#include "MLTADFPass.h"

class KELPPass: public MLTADFPass {
private:
    set<CallInst*> simpleIndCalls;
    set<Function*> confinedAddrTakenFuncs;

public:
    KELPPass(GlobalContext *Ctx_): MLTADFPass(Ctx_){
        ID = "kelp analysis";
    }

    virtual bool doInitialization(Module*) override;

    virtual bool doFinalization(Module *M) override;

    virtual void analyzeIndCall(CallInst* CI, FuncSet* FS) override;
};

#endif //INDIRECTCALLANALYZER_KELPPASS_H
