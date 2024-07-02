//
// Created by prophe cheng on 2024/1/3.
//

#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Operator.h>
#include <regex>
#include <utility>
#include "Common.h"
#include "Config.h"

// Map from struct elements to its name
static map<string, set<StringRef>> elementsStructNameMap;

bool CommonUtil::trimPathSlash(string &path, int slash) {
    while (slash > 0) {
        path = path.substr(path.find('/') + 1);
        --slash;
    }

    return true;
}

string CommonUtil::getFileName(DILocation *Loc, DISubprogram *SP) {
    string FN;
    if (Loc)
        FN = Loc->getFilename().str();
    else if (SP)
        FN = SP->getFilename().str();
    else
        return "";

    int slashToTrim = 2;
    char *user = getlogin();
    if (strstr(user, "kjlu")) {
        slashToTrim = 0;
        trimPathSlash(FN, slashToTrim);
    }
    else {
        // OP << "== Warning: please specify the path of linux source.";
    }
    return FN;
}

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
    if (typeName2newHash.find(valid_struct_name) != typeName2newHash.end()
        && typeName2newHash[valid_struct_name].size() == 1)
        struct_name = valid_struct_name;
    return struct_name;
}



StringRef CommonUtil::getCalledFuncName(CallInst *CI) {
    Value *V;
    V = CI->getCalledOperand();
    assert(V);

    InlineAsm *IA = dyn_cast<InlineAsm>(V);
    if (IA)
        return StringRef(IA->getAsmString());

    User *UV = dyn_cast<User>(V);
    if (UV) {
        if (UV->getNumOperands() > 0) {
            Value *VUV = UV->getOperand(0);
            return VUV->getName();
        }
    }

    return V->getName();
}

// 获取指令I的源代码位置信息
DILocation* CommonUtil::getSourceLocation(Instruction *I) {
    if (!I)
        return NULL;

    MDNode *N = I->getMetadata("dbg");
    if (!N)
        return NULL;

    DILocation *Loc = dyn_cast<DILocation>(N);
    if (!Loc || Loc->getLine() < 1)
        return NULL;

    return Loc;
}

// 获取函数F的第ArgNo个参数对象
Argument* CommonUtil::getParamByArgNo(Function *F, int8_t ArgNo) {
    if (ArgNo >= F->arg_size())
        return NULL;

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

// 计算结构体类型的hash值
string CommonUtil::structTypeHash(StructType *STy, set<size_t> &HSet) {
    hash<string> str_hash;
    string sig;
    string ty_str;
    string struct_name;
    // TODO: Use more but reliable information
    // FIXME: A few cases may not even have a name
    // 如果不是匿名结构体，直接根据名字计算hash
    if (STy->hasName()) { // 获取该层结构体对应的type name的hash
        // struct_name示例struct.ngx_core_module_t，可能以数字后缀结尾
        struct_name = getValidStructName(STy);
        HSet.insert(str_hash(struct_name));
    }
    // 如果是匿名结构体，根据每个元素的类型计算hash
    else {
        string sstr = structTyStr(STy);
        struct_name = "Annoymous struct: " + sstr;
        // 如果在elementsStructNameMap中找到了该匿名结构体的信息，也就是存在实名结构体field结构和该匿名结构体一致
        if (elementsStructNameMap.find(sstr) != elementsStructNameMap.end()) {
            for (auto SStr : elementsStructNameMap[sstr]) {
                ty_str = SStr.str();
                // 同时添加下实名结构体的hash值
                HSet.insert(str_hash(ty_str));
            }
        }
    }
    return struct_name;
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
        if (STy->hasName())
            ty_str = getValidStructName(STy);
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

size_t CommonUtil::strIntHash(string str, int i) {
    hash<string> str_hash;
    // FIXME: remove pos
    size_t pos = str.rfind("/");
    return str_hash(str.substr(0, pos) + to_string(i));
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