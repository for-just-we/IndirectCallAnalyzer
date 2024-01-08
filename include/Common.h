//
// Created by prophe cheng on 2024/1/3.
//

#ifndef TYPEDIVE_COMMON_H
#define TYPEDIVE_COMMON_H

#include <llvm/IR/Module.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/ADT/Triple.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/IR/DebugInfo.h>

#include <unistd.h>
#include <bitset>
#include <chrono>
#include <set>

using namespace llvm;
using namespace std;

#define OP llvm::errs()
#define DBG if (debug_mode) OP

//
// Common functions
//
string getFileName(DILocation *Loc,
                   DISubprogram *SP=NULL);

StringRef getCalledFuncName(CallInst *CI);

// 获取指令I的源代码位置信息
DILocation *getSourceLocation(Instruction *I);

// 获取函数F的第ArgNo个参数对象
Argument *getParamByArgNo(Function *F, int8_t ArgNo);

// 根据函数F的FunctionType (返回类型、参数类型、是否支持可变参数)计算F的hash值
size_t funcHash(Function *F, bool withName = false);

// 根据callsite对应的FunctionType计算hash
size_t callHash(CallInst *CI);

// 计算Struct Type的hash值
string structTypeHash(StructType *STy, set<size_t> &HSet);

size_t typeHash(Type *Ty);

size_t typeIdxHash(Type *Ty, int Idx = -1);

size_t hashIdxHash(size_t Hs, int Idx = -1);

size_t strIntHash(string str, int i);

string structTyStr(StructType *STy);

bool trimPathSlash(string &path, int slash);

int64_t getGEPOffset(const Value *V, const DataLayout *DL);

// 从所有模块加载结构体信息，初始化使用
void LoadElementsStructNameMap(vector<pair<Module*, StringRef>> &Modules);


class Helper {
public:
    // LLVM value
    static string getValueName(Value *v) {
        if (!v->hasName()) {
            return to_string(reinterpret_cast<uintptr_t>(v));
        } else {
            return v->getName().str();
        }
    }

    static string getValueType(Value *v) {
        if (Instruction *inst = dyn_cast<Instruction>(v)) {
            return string(inst->getOpcodeName());
        } else {
            return string("value " + to_string(v->getValueID()));
        }
    }

    static string getValueRepr(Value *v) {
        string str;
        raw_string_ostream stm(str);

        v->print(stm);
        stm.flush();

        return str;
    }
};

#endif //TYPEDIVE_COMMON_H
