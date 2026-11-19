#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Intrinsics.h"

#include "util.h"
#include "SubPass.h"
#include "out.h"
#include "SSLH.h"

#include <utility>
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <numeric>

using namespace llvm;
using namespace std;



PreservedAnalyses SSLHPass::run(Module &M, ModuleAnalysisManager &AM) {
    std::set<Function *> worklist;
    std::set<LeakPoint> leakset;
    for (auto &func : M.functions()){
        worklist.insert(&func);
        for(auto &bb : func){
            for(auto &inst: bb){
                if(isa<LoadInst>(&inst)){
                    auto load = dyn_cast<LoadInst>(&inst);
                    leakset.insert(LeakPoint(load->getPointerOperand(), &bb, LeakType::HighDependentMemorryAccessLoad));
                }
                if(isa<StoreInst>(&inst)){
                    auto store = dyn_cast<StoreInst>(&inst);
                    leakset.insert(LeakPoint(store->getPointerOperand(), &bb, LeakType::HighDependentMemorryAccessStore));
                }
                if(isa<BranchInst>(&inst)){
                    auto branch = dyn_cast<BranchInst>(&inst);
                    if(branch->isConditional()){
                        leakset.insert(LeakPoint(branch->getCondition(), &bb, LeakType::HighDependentBranch));
                    }
                }
            }
        }
    }
    repairLeakPoint(M, leakset, worklist);
    return PreservedAnalyses::all();
}



