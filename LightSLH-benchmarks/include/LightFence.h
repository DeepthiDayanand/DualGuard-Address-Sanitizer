#ifndef LIGHT_FENCE_H
#define LIGHT_FENCE_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/IR/Instruction.h"

#include <map>
#include <iostream>
#include<fstream>
#include <unordered_set>

#include "AbstractDomain.h"
#include "util.h"
#include "LightHardening.h"

struct LightFencePass : public LightHardeningPass {
    llvm::PreservedAnalyses run(llvm::Module &M,
                                llvm::ModuleAnalysisManager &AM) override;

    State runOnLoadInst(llvm::LoadInst *, LightHardeningPass::Mode ) override;
    void runOnStoreInst(llvm::StoreInst *, LightHardeningPass::Mode ) override;
    void runOnBranchInst(llvm::BranchInst *, LightHardeningPass::Mode ) override;

    std::unordered_set<llvm::BasicBlock *> fence_set;
};

#endif