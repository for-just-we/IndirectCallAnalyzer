//
// Created by prophe cheng on 2024/1/3.
//

#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Operator.h>
#include <regex>
#include <utility>
#include "Utils/Common.h"

// Map from struct elements to its name
map<string, set<StringRef>> CommonUtil::elementsStructNameMap;

// ToDo：考虑匿名结构体
string CommonUtil::getValidStructName(string structName) {
    // 查找最后一个点的位置
    size_t lastDotPos = structName.find_last_of('.');

    // 如果 structName 中包含 "anno"，直接返回 structName
    if (structName.find("anon") != std::string::npos)
        return structName;

    // 如果没有点，或者点后面不是数字，则直接返回原始字符串
    if (lastDotPos == std::string::npos || !isdigit(structName[lastDotPos + 1]))
        return structName;

    // 返回去除数字后缀的字符串
    return structName.substr(0, lastDotPos);
}

string CommonUtil::getValidStructName(StructType *STy) {
    string struct_name = STy->getName().str();
    string valid_struct_name = getValidStructName(struct_name);
    return valid_struct_name;
}

// 获取函数F的第ArgNo个参数对象
Argument* CommonUtil::getParamByArgNo(Function* F, int8_t ArgNo) {
    if (ArgNo >= F->arg_size())
        return nullptr;

    int8_t idx = 0;
    Function::arg_iterator ai = F->arg_begin();
    while (idx != ArgNo) {
        ++ai;
        ++idx;
    }
    return ai;
}

// 从所有模块加载结构体信息，初始化使用
void CommonUtil::LoadElementsStructNameMap(vector<pair<Module*, StringRef>> &Modules) {
    // 遍历所有的模块
    for (auto M : Modules) {
        // 遍历所有非匿名结构体
        for (auto STy : M.first->getIdentifiedStructTypes()) {
            assert(STy->hasName());
            if (STy->isOpaque()) // 必须有定义，不能只是声明
                continue;

            string strSTy = structTyStr(STy); // 基于每个成员变量类型构建该类型的key
            elementsStructNameMap[strSTy].insert(STy->getName());
        }
    }
}

void cleanString(string &str) {
    // process string
    // remove c++ class type added by compiler
    size_t pos = str.find("(%class.");
    if (pos != string::npos) {
        //regex pattern1("\\(\\%class\\.[_A-Za-z0-9]+\\*,?");
        regex pattern("^[_A-Za-z0-9]+\\*,?");
        smatch match;
        string str_sub = str.substr(pos + 8);
        if (regex_search(str_sub, match, pattern)) {
            str.replace(pos + 1, 7 + match[0].length(), "");
        }
    }
    string::iterator end_pos = remove(str.begin(), str.end(), ' ');
    str.erase(end_pos, str.end());
}

// 计算函数F的has值
size_t CommonUtil::funcHash(Function *F, bool withName) {
    hash<string> str_hash;
    string output;

    string sig;
    raw_string_ostream rso(sig);
    // FunctionType包含返回值类型，参数类型，是否支持可变参数 3个信息
    FunctionType *FTy = F->getFunctionType();
    FTy->print(rso);
    // output为FunctionType的tostring
    output = rso.str();

    if (withName)
        output += F->getName();
    // process string
    cleanString(output);

    return str_hash(output);
}


// 获取callsite对应的signature
size_t CommonUtil::callHash(CallInst *CI) {
    CallBase *CB = dyn_cast<CallBase>(CI);

    hash<string> str_hash;
    string sig;
    raw_string_ostream rso(sig);
    // 根据callsite对应的FunctionType计算hash
    FunctionType *FTy = CB->getFunctionType();
    FTy->print(rso);
    string strip_str = rso.str();
    //string strip_str = funcTypeString(FTy);
    cleanString(strip_str);

    return str_hash(strip_str);
}


string CommonUtil::structTyStr(StructType *STy) {
    string ty_str;
    string sig;
    for (auto Ty : STy->elements()) {
        ty_str += to_string(Ty->getTypeID());
    }
    return ty_str;
}

// 计算类型hash,
// 相比原版mlta，我们对structTypeHash做一些调整，参考https://blog.csdn.net/fcsfcsfcs/article/details/119062032
size_t CommonUtil::typeHash(Type *Ty) {
    hash<string> str_hash;
    string sig;
    string ty_str;

    // 如果是结构体类型
    if (StructType *STy = dyn_cast<StructType>(Ty)) {
        // TODO: Use more but reliable information
        // FIXME: A few cases may not even have a name
        if (STy->hasName()) {
            ty_str = getValidStructName(STy);
            ty_str += ("," + itostr(STy->getNumElements()));
        }
        else {
            string sstr = structTyStr(STy);
            if (elementsStructNameMap.find(sstr) != elementsStructNameMap.end())
                ty_str = elementsStructNameMap[sstr].begin()->str();
        }
    }
    else {
        raw_string_ostream rso(sig);
        Ty->print(rso);
        ty_str = rso.str();
        string::iterator end_pos = remove(ty_str.begin(), ty_str.end(), ' ');
        ty_str.erase(end_pos, ty_str.end());
    }
    return str_hash(ty_str);
}

size_t CommonUtil::hashIdxHash(size_t Hs, int Idx) {
    hash<string> str_hash;
    return Hs + str_hash(to_string(Idx));
}

size_t CommonUtil::typeIdxHash(Type *Ty, int Idx) {
    return hashIdxHash(typeHash(Ty), Idx);
}

int64_t CommonUtil::getGEPOffset(const Value *V, const DataLayout *DL) {
    const GEPOperator *GEP = dyn_cast<GEPOperator>(V);

    int64_t offset = 0;
    const Value *baseValue = GEP->getPointerOperand()->stripPointerCasts();
    if (const ConstantExpr *cexp = dyn_cast<ConstantExpr>(baseValue))
        if (cexp->getOpcode() == Instruction::GetElementPtr)
        {
            // FIXME: this looks incorrect
            offset += getGEPOffset(cexp, DL);
        }
    Type *ptrTy = GEP->getSourceElementType();

    SmallVector<Value *, 4> indexOps(GEP->op_begin() + 1, GEP->op_end());
    // Make sure all indices are constants
    for (unsigned i = 0, e = indexOps.size(); i != e; ++i)
    {
        if (!isa<ConstantInt>(indexOps[i]))
            indexOps[i] = ConstantInt::get(Type::getInt32Ty(ptrTy->getContext()), 0);
    }
    offset += DL->getIndexedOffsetInType(ptrTy, indexOps);
    return offset;
}


Function* CommonUtil::getBaseFunction(Value *V) {
    if (Function *F = dyn_cast<Function>(V))
        if (F != nullptr)
            return F;
    Value *CV = V;
    while (BitCastOperator *BCO = dyn_cast<BitCastOperator>(CV)) {
        Value* O = BCO->getOperand(0);
        if (Function *F = dyn_cast<Function>(O))
            if (F != nullptr)
                return F;
        CV = O;
    }
    return NULL;
}


string getInstructionText(Value* inst) {
    if (Function* F = dyn_cast<Function>(inst))
        return F->getName().str();
    string instructionText;
    raw_string_ostream stream(instructionText);
    inst->print(stream);
    stream.flush();
    return instructionText;
}

string getInstructionText(Type* type) {
    string instructionText;
    raw_string_ostream stream(instructionText);
    type->print(stream);
    stream.flush();
    return instructionText;
}

string getValueInfo(Value* value) {
    string inst_name;
    raw_string_ostream rso(inst_name);
    value->print(rso);
    return rso.str();
}

string getTypeInfo(Type* type) {
    string inst_name;
    raw_string_ostream rso(inst_name);
    type->print(rso);
    return rso.str();
}