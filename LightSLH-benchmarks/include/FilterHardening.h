#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

#include "util.h"

using namespace llvm;

struct FilterHardeningPass : public PassInfoMixin<FilterHardeningPass> {
    std::set<LeakPoint> protect_set;
    std::set<Function *> work_function_set;

    virtual PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
    void initialize(Module &M);
};