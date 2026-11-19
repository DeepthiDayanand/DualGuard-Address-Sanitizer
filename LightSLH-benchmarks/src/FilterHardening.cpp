#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/InlineAsm.h"

#include "FilterHardening.h"
#include "SubPass.h"

using namespace llvm;

PreservedAnalyses FilterHardeningPass::run(Module &M, ModuleAnalysisManager &AM) {
    initialize(M);
    errs() << "Hardening Size:" << protect_set.size() << "\n";
    for (const auto &leak : protect_set) {
        errs() << "Leak Point: " << *leak.val << "\n";
    }
    errs() << "Work Function Size:" << work_function_set.size() << "\n";
    for (const auto &func : work_function_set) {
        errs() << "Work Function: " << func->getName() << "\n";
    }

    repairLeakPoint(M, protect_set, work_function_set);
    return PreservedAnalyses::all();
}

void FilterHardeningPass::initialize(Module &M) {
    int marked = 0;
    int protect = 0;
    for (auto &F : M) {
        for (auto &BB : F) {
            for (auto It = BB.begin(), End = BB.end(); It != End;) {
                Instruction *I = &*It++;
                // GEPs
                if (CallBase *call = dyn_cast<CallBase>(I)) {
                    if (InlineAsm *dummyAsm = dyn_cast<InlineAsm>(call->getCalledOperand())) {
                        std::string asmString = dummyAsm->getAsmString();
                        if (asmString.find("addq 123456, %rax") != std::string::npos) {
                            marked++;
                            work_function_set.insert(&F);
                            auto gep = I->getNextNode();
                            auto endDummy = gep->getNextNode();
                            auto users = gep->users();
                            for (auto user : users) {
                                if (isa<LoadInst>(user)) {
                                    protect_set.insert(LeakPoint{gep, gep->getParent(), LeakType::HighDependentMemorryAccessLoad});
                                    protect++;
                                } else if (isa<StoreInst>(user)) {
                                    protect_set.insert(LeakPoint{gep, gep->getParent(), LeakType::HighDependentMemorryAccessStore});
                                    protect++;
                                } else {
                                    errs() << "Unexpected user type: " << *user << "\n";
                                    errs() << "GEP: " << *gep << "\n";
                                }
                            }
                            I->eraseFromParent();
                            endDummy->eraseFromParent();
                        }
                    }
                } 
                // branches
                else if (BranchInst *brInst = dyn_cast<BranchInst>(I)) {
                    if (brInst->getMetadata("taint.branch")) {
                        work_function_set.insert(&F);
                        protect_set.insert(LeakPoint(brInst->getCondition(), brInst->getParent(), LeakType::HighDependentBranch));
                    }
                    
                }
            }
        }
    }
    errs() << "Marked: " << marked << "\n";
    errs() << "Protected: " << protect << "\n";
    errs() << "Marked but not protected: " << marked - protect << "\n";
}