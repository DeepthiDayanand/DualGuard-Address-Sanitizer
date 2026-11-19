#ifndef LIGHT_V3_H
#define LIGHT_V3_H

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

struct LightV3Pass : public LightHardeningPass {
    llvm::PreservedAnalyses run(llvm::Module &M,
                                llvm::ModuleAnalysisManager &AM) override;

};


#endif