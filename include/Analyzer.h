//
// Created by prophe cheng on 2024/1/3.
//

#ifndef TYPEDIVE_ANALYZER_H
#define TYPEDIVE_ANALYZER_H

#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Support/CommandLine.h>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "Common.h"




// 主要关注3个函数，doInitialization、doFinalization、doModulePass
// doInitialization完成type-hierachy分析以及type-propagation
// doFinalization将结果dump出
// doModulePass进行间接调用分析
class IterativeModulePass {
protected:
    const char * ID;
public:
    IterativeModulePass(GlobalContext *Ctx_, const char *ID_)
            : ID(ID_) { }

    // Run on each module before iterative pass.
    virtual bool doInitialization(llvm::Module *M) {
        return true;
    }

    // Run on each module after iterative pass.
    virtual bool doFinalization(llvm::Module *M) {
        return true;
    }

    // Iterative pass.
    virtual bool doModulePass(llvm::Module *M) {
        return false;
    }

    virtual void run(ModuleList &modules) {
        ModuleList::iterator i, e;
        OP << "[" << ID << "] Initializing " << modules.size() << " modules ";
        bool again = true;
        while (again) {
            again = false;
            for (i = modules.begin(), e = modules.end(); i != e; ++i) {
                again |= doInitialization(i->first); // type-hierachy and type-propagation
                OP << ".";
            }
        }
        OP << "\n";

        unsigned iter = 0, changed = 1;
        while (changed) {
            ++iter;
            changed = 0;
            unsigned counter_modules = 0;
            unsigned total_modules = modules.size();
            for (i = modules.begin(), e = modules.end(); i != e; ++i) {
                OP << "[" << ID << " / " << iter << "] ";
                OP << "[" << ++counter_modules << " / " << total_modules << "] ";
                OP << "[" << i->second << "]\n";

                bool ret = doModulePass(i->first);
                if (ret) {
                    ++changed;
                    OP << "\t [CHANGED]\n";
                } else
                    OP << "\n";
            }
            OP << "[" << ID << "] Updated in " << changed << " modules.\n";
        }

        OP << "[" << ID << "] Postprocessing ...\n";
        again = true;
        while (again) {
            again = false;
            for (i = modules.begin(), e = modules.end(); i != e; ++i) {
                // TODO: Dump the results.
                again |= doFinalization(i->first);
            }
        }

        OP << "[" << ID << "] Done!\n\n";
    }

};

#endif //TYPEDIVE_ANALYZER_H
