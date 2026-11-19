#ifndef SSLH_H
#define SSLH_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/IR/Instruction.h"

#include <map>
#include <iostream>
#include<fstream>
#include <unordered_set>

#include "util.h"


struct SSLHPass : public llvm::PassInfoMixin<SSLHPass> {
    llvm::PreservedAnalyses run(llvm::Module &M,
                                llvm::ModuleAnalysisManager & AM);
};

#endif