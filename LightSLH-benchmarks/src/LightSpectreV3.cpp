#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Intrinsics.h"

#include "LightHardening.h"
#include "LightSpectreV3.h"
#include "util.h"
#include "SubPass.h"
#include "out.h"

#include <utility>
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <numeric>
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"

using namespace llvm;
using namespace std;


PreservedAnalyses LightV3Pass::run(Module &M, ModuleAnalysisManager &AM) {
    std::unordered_set<llvm::BasicBlock *> block_bb;

    for(Function &F: M){
        if (!F.empty()) {
            llvm::BasicBlock* entryBB = &(F.getEntryBlock());
            if(entryBB){
                block_bb.insert(entryBB);
            }
        }
    }
    repairLeakPointFence(M, block_bb);
    return PreservedAnalyses::all();
}