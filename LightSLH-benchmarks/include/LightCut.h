#ifndef LIGHT_CUT_H
#define LIGHT_CUT_H

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

struct LightCutPass : public LightHardeningPass {
    llvm::PreservedAnalyses run(llvm::Module &M,
                                llvm::ModuleAnalysisManager &AM) override;

    State runOnLoadInst(llvm::LoadInst *, LightHardeningPass::Mode ) override;
    void runOnStoreInst(llvm::StoreInst *, LightHardeningPass::Mode ) override;
    void runOnBranchInst(llvm::BranchInst *, LightHardeningPass::Mode ) override;
    void preProcessBasicBlock(llvm::BasicBlock* bb) override;
    void preProcessInstruction(llvm::Instruction* bb) override;
    void setBlockBB();
    bool findSinkBB(llvm::BasicBlock* source, llvm::BasicBlock* curbb);

    std::map<llvm::Function*, llvm::LoopInfo*> func2li;
    // std::set<llvm::Instruction *> fence_set;
    std::unordered_set<llvm::BasicBlock *> block_bb;
    std::unordered_set<llvm::BasicBlock *> source_bb;
    std::unordered_set<llvm::BasicBlock *> sink_bb;

    std::unordered_map<llvm::BasicBlock*, std::unordered_set<llvm::BasicBlock*>> affected_bb;
    std::unordered_map<llvm::Instruction*, std::unordered_set<llvm::BasicBlock*>> inst2oobs;
    

};

#endif