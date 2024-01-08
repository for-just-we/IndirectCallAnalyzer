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

// 常用类型定义
typedef std::vector<std::pair<llvm::Module*, llvm::StringRef>> ModuleList; // 模块列表类型，每个模块对应一个Module*对象以及一个模块名
// Mapping module to its file name.
typedef std::unordered_map<llvm::Module*, llvm::StringRef> ModuleNameMap; // 将模块对象映射为模块名的类型
// The set of all functions.
typedef llvm::SmallPtrSet<llvm::Function*, 8> FuncSet; // 函数集合类型
typedef llvm::SmallPtrSet<llvm::CallInst*, 8> CallInstSet; // Call指令集合类型
typedef DenseMap<Function*, CallInstSet> CallerMap; // 将Function对象映射为对应的callsite集合
typedef DenseMap<CallInst *, FuncSet> CalleeMap; // 将Call指令映射为对应的函数集合

// 保存中间及最终结果的结构体
struct GlobalContext {
    GlobalContext() {}

    // Statistics
    unsigned NumFunctions = 0;
    unsigned NumFirstLayerTypeCalls = 0;
    unsigned NumSecondLayerTypeCalls = 0;
    unsigned NumSecondLayerTargets = 0;
    unsigned NumValidIndirectCalls = 0;
    unsigned NumIndirectCallTargets = 0;
    unsigned NumFirstLayerTargets = 0;

    // 全局变量，将变量的hash值映射为变量对象，只保存有initializer的全局变量
    DenseMap<size_t, GlobalVariable*> Globals;

    // 将一个global function的id(uint64_t) 映射到实际Function对象.
    map<uint64_t, Function*> GlobalFuncMap;

    // address-taken函数集合
    FuncSet AddressTakenFuncs;

    // 将一个indirect-callsite映射到target function集合，Map a callsite to all potential callee functions.
    CalleeMap Callees;

    // 将一个function映射到对应的indirect-callsite caller集合.
    CallerMap Callers;

    // 将一个函数签名映射为对应函数集合s
    DenseMap<size_t, FuncSet> sigFuncsMap;

    // Indirect call instructions.
    std::vector<CallInst *>IndirectCallInsts;

    // Modules.
    ModuleList Modules;
    ModuleNameMap ModuleMaps;
    std::set<std::string> InvolvedModules;
};

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
