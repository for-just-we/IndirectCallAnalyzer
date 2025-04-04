//
// Created by prophe cheng on 2025/4/4.
//

#ifndef INDIRECTCALLANALYZER_MLTADFPASS_H
#define INDIRECTCALLANALYZER_MLTADFPASS_H

#include "MLTAPass.h"
#include "Utils/Config.h"

class MLTADFPass: public MLTAPass {
public:
    MLTADFPass(GlobalContext *Ctx_): MLTAPass(Ctx_){
        ID = "data flow enhanced multi layer type analysis";
    }

    bool typeConfineFromCI(Function* CF, unsigned ArgNo, Function* FF) override;

    void typeConfineFromCIRecursive(Function* CF, unsigned ArgNo, Function* FF, set<Function*>& visitedFunc);
};


#endif //INDIRECTCALLANALYZER_MLTADFPASS_H
