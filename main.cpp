#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/Path.h"

#include <memory>
#include <iostream>
#include <string>


#include "Analyzer.h"
#include "CallGraph.h"
#include "Config.h"

using namespace llvm;
using namespace std;

// Command line parameters.
cl::list<std::string> InputFilenames(
        cl::Positional,
        cl::OneOrMore,
        cl::desc("<input bitcode files>"));

// 默认采用FLTA
cl::opt<bool> MLTA(
        "mlta",
        cl::desc("Multi-layer type analysis for refining indirect-call targets"),
        cl::NotHidden, cl::init(false));

// 默认逐个匹配类型而不是比对函数签名
cl::opt<bool> SIG_MATCH(
        "sig_match",
        cl::desc("Use signature match when use FLTA. Default compare argument of each type."),
        cl::NotHidden, cl::init(false));

cl::opt<bool> DebugMode(
        "debug",
        cl::desc("debug mode"),
        cl::init(false)
);

// 结果保存路径
cl::opt<string> OutputFilePath(
        "output-file",
        cl::desc("Output file path, better to use absolute path"),
        cl::init("results.txt"));

GlobalContext GlobalCtx;

// 打印结果
void PrintResults(GlobalContext *GCtx) {
    int TotalTargets = 0;
    // 计算间接调用总共调用的target function数量
    for (auto IC : GCtx->IndirectCallInsts)
        TotalTargets += GCtx->Callees[IC].size();

    int totalsize = 0;
    OP << "\n@@ Total number of final callees: " << totalsize << ".\n";

    OP<<"############## Result Statistics ##############\n";
    //cout<<"# Ave. Number of indirect-call targets: \t"<<std::setprecision(5)<<AveIndirectTargets<<"\n";
    OP<<"# Number of indirect calls: \t\t\t"<<GCtx->IndirectCallInsts.size()<<"\n";
    OP<<"# Number of indirect calls with targets: \t"<<GCtx->NumValidIndirectCalls<<"\n";
    OP<<"# Number of indirect-call targets: \t\t"<<GCtx->NumIndirectCallTargets<<"\n";
    OP<<"# Number of address-taken functions: \t\t"<<GCtx->AddressTakenFuncs.size()<<"\n";
    OP<<"# Number of multi-layer calls: \t\t\t"<<GCtx->NumSecondLayerTypeCalls<<"\n";
    OP<<"# Number of multi-layer targets: \t\t"<<GCtx->NumSecondLayerTargets<<"\n";
    OP<<"# Number of one-layer calls: \t\t\t"<<GCtx->NumFirstLayerTypeCalls<<"\n";
    OP<<"# Number of one-layer targets: \t\t\t"<<GCtx->NumFirstLayerTargets<<"\n";

    // 将结果保存到文件
    std::ofstream outputFile(OutputFilePath);

    for (auto &curEle: GCtx->Callees) {
        if (curEle.first->isIndirectCall()) {
            totalsize += curEle.second.size();
            FuncSet funcs = curEle.second;

            auto *Scope = cast<DIScope>(curEle.first->getDebugLoc().getScope());
            string callsiteFile = Scope->getFilename().str();
            int line = curEle.first->getDebugLoc().getLine();
            int col = curEle.first->getDebugLoc().getCol();
            string content = callsiteFile + ":" + itostr(line) + ":" + itostr(col) + "|";
            for (llvm::Function* func: funcs)
                content += (func->getName().str() + ",");
            content = content.substr(0, content.size() - 1);
            content += "\n";
            outputFile << content;
        }
    }
    outputFile.close();
}


int main(int argc, char** argv) {
    // Print a stack trace if we signal out.
    sys::PrintStackTraceOnErrorSignal(argv[0]);
    PrettyStackTraceProgram X(argc, argv);

    llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.

    cl::ParseCommandLineOptions(argc, argv, "global analysis\n");
    SMDiagnostic Err;

    for (unsigned i = 0; i < InputFilenames.size(); ++i) {
        LLVMContext *LLVMCtx = new LLVMContext();
        std::unique_ptr<Module> M = parseIRFile(InputFilenames[i], Err, *LLVMCtx);

        if (M == NULL) {
            OP << argv[0] << ": error loading file '" << InputFilenames[i] << "'\n";
            continue;
        }

        Module *Module = M.release();
        StringRef MName = StringRef(strdup(InputFilenames[i].data()));
        GlobalCtx.Modules.push_back(std::make_pair(Module, MName));
        GlobalCtx.ModuleMaps[Module] = InputFilenames[i];
    }

    ENABLE_MLTA = MLTA;
    ENABLE_SIGMATCH = SIG_MATCH;
    debug_mode = DebugMode;

    // 进行indirect-call分析
    CallGraphPass CGPass(&GlobalCtx);
    CGPass.run(GlobalCtx.Modules);

    // 打印分析结果
    PrintResults(&GlobalCtx);
    return 0;
}
