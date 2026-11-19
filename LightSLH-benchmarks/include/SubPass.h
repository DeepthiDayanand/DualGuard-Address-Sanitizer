#ifndef SUBPASS_H
#define SUBPASS_H

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/MDBuilder.h"

#include "util.h"
#include "AbstractDomain.h"

#include <map>
#include <set>

std::set<llvm::Function *> collectFunctions(llvm::Function *, std::set<llvm::Function *>& skip_set);
std::map<llvm::Value *, llvm::Value *> collectConstraintVariable(std::set<llvm::Function *> &);
std::map<llvm::Value *, llvm::Value *> collectMemBaseMap(llvm::Module &M, std::set<llvm::Function *> &functions, std::set<MemBase*> &);
BaseAddressMap collectMemorySymbol(llvm::Module &, MemStateMap &, std::map<llvm::Value *, llvm::Value *>&,  std::set<llvm::Function *> &);
LoopVariableSet collectLoopVariable(std::set<llvm::Function*>& funcs);
std::map<llvm::BasicBlock *, llvm::Value *> collectBlockControlLoopVariable(std::set<llvm::Function *> &funcs);

std::map<llvm::Value *, llvm::Value *> setSpeculativeIndicator(llvm::Module &M, std::set<llvm::BasicBlock *> &leakblock);
void repairLeakPoint(llvm::Module &M, std::set<LeakPoint> &leakset, std::set<llvm::Function*>& funcs);
void repairLeakPointFence(llvm::Module &M, std::set<llvm::Instruction*> &leakset);
void repairLeakPointFence(llvm::Module &M, std::unordered_set<llvm::BasicBlock*> &leakset);


llvm::Value *readSpeculativeFlagFromRSP(llvm::IRBuilder<> &, llvm::Instruction *);
void setSpeculativeIndicatorToRSP(llvm::IRBuilder<>  &, llvm::Instruction *, llvm::Value *);

#endif