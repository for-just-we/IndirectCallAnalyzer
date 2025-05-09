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
    map<string, int> sysAPIs = {
            {"pthread_create", 2},
            {"thread", 0},
            {"signal", 1},
            {"qsort", 2},
            {"bsearch", 2},
    };

public:
    KELPPass(GlobalContext *Ctx_): MLTADFPass(Ctx_){
        ID = "kelp analysis";
    }

    bool doInitialization(Module*) override;

    bool doFinalization(Module *M) override;

    void analyzeIndCall(CallInst* CI, FuncSet* FS) override;
};

#endif //INDIRECTCALLANALYZER_KELPPASS_H
