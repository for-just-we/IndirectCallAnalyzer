//
// Created by prophe cheng on 2025/4/4.
//

#include "Passes/MLTADFPass.h"


bool MLTADFPass::typeConfineFromCI(Function* CF, unsigned ArgNo, Function* FF) {
    set<Function*> visitedFuncs;
    typeConfineFromCIRecursive(CF, ArgNo, FF, visitedFuncs);
    return true;
}

void MLTADFPass::typeConfineFromCIRecursive(Function* CF, unsigned ArgNo, Function* FF, set<Function*>& visitedFunc) {
    if (visitedFunc.count(CF))
        return;
    visitedFunc.insert(CF);
    if (Argument* Arg = Ctx->util.getParamByArgNo(CF, ArgNo)) { // CF为被调用的函数，这里返回函数指针对应的形参
        // U为函数CF中使用了该形参的指令
        // 遍历所有使用形参的指令
        for (auto U: Arg->users()) {
            // 如果使用形参的是store指令或者bitcast指令
            if (StoreInst* SI = dyn_cast<StoreInst>(U)) {
                confineTargetFunction(SI->getPointerOperand(), FF);
                OP << "confine: " << FF->getName().str() << " to: " << getInstructionText(SI->getPointerOperand()) << "\n";
            }

            else if (isa<BitCastOperator>(U)) {
                confineTargetFunction(U, FF);
                OP << "confine: " << FF->getName().str() << " to: " << getInstructionText(U) << "\n";
            }
            else if (CallInst* CI = dyn_cast<CallInst>(U)) {
                // this should be done in Kelp
                Value* CV = CI->getCalledOperand();
                if (CV == Arg)
                    continue;
                Function* _CF = dyn_cast<Function>(CV); // CF为被调用的函数
                // 如果不是间接调用，接着分析
                if (!_CF)
                    continue;
                // call target
                if (_CF->isDeclaration())
                    CF = Ctx->GlobalFuncMap[CF->getGUID()];
                if (!_CF)
                    continue;
                OP << "analyzing: " << FF->getName().str() << " to call: " << getInstructionText(CI) << "\n";
                for (User::op_iterator OI = CI->op_begin(), OE = CI->op_end(); OI != OE; ++OI) {
                    if (*OI == Arg)
                        typeConfineFromCIRecursive(_CF, OI->getOperandNo(), FF, visitedFunc);
                }
            }
        }
    }
}