#ifndef LIGHT_HARDENING_H
#define LIGHT_HARDENING_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/IR/Instruction.h"

#include <map>
#include <iostream>
#include<fstream>
#include <unordered_set>

#include "AbstractDomain.h"
#include "util.h"


struct LightHardeningPass : public llvm::PassInfoMixin<LightHardeningPass> {
    virtual llvm::PreservedAnalyses run(llvm::Module &M,
                                llvm::ModuleAnalysisManager & AM);
    enum Mode{
        SEQ,
        SPEC
    };

    std::set<const llvm::Instruction*> oobLoads;
    std::set<const llvm::Instruction*> oobStores;
    std::set<const llvm::Instruction*> taintedGEPs;
    std::map<std::string, std::set<int>> sourceLines;

    std::set<const llvm::Instruction*> totalGEPsLightSLH;
    std::set<const llvm::Instruction*> oobGEPs;
    std::set<const llvm::Value*> oobPtrNotGEPs;
    std::set<const llvm::Instruction*> oobGEPsWithoutDebug;

    int func_count = 0;
    int inst_count = 0;
    struct BasicBlockMessage{
        MemStateMap block_in_spec;   // map block to its IN state under speculative excution
        MemStateMap block_out_spec;  // map block to its OUT state under speculative excution
        MemStateMap block_in_seq;   // map block to its IN state under squential excution
        MemStateMap block_out_seq;  // map block to its OUT state under squential excution
        MemStateMap block_out_seq_final;  // record the highest value it can reach
    };

    struct RegMessage{
        State state_spec;
        State state_seq_runtime;      //runtime value, will be cleared and recomputed every time its function is called
        State state_seq_final;        // final value, record the highest value it can reach

        RegMessage(size_t size){
            state_spec = State(size);
            state_seq_runtime = State(size);
            state_seq_final  = State(size);
        }
        RegMessage(){}
    };

    llvm::Module *current_module;
    const llvm::DataLayout *DL;
    llvm::Function* entry_function;
    MemStateMap initMem2StateMap;     //initial memory(need not contain all memory symbol)
    
    uint cache_base = 0;
    bool fence;
    bool constraint_flag = false;

    llvm::BasicBlock *current_block;        // used when need to access block constraint
    State tempState; // used for return a ref of State but need to create a new one

    std::map<llvm::Value *, llvm::Value *> membase_map; // map malloc to a base with a fine type;
    BaseAddressMap mem_map;                             // map base address to its size;
    std::map<llvm::BasicBlock *, BasicBlockMessage> bb2message;
    std::map<llvm::BasicBlock *, RegConstraintMap> block_in_constraint; // map block to its reach-condition
    std::map<llvm::BasicBlock *, RegConstraintMap> block_in_constraint_final; // map block to its reach-condition
    std::set<llvm::BasicBlock *> seq_reachable_block;

    std::map<Register*, RegMessage> reg2message;
    std::map<llvm::Instruction *, MemStateMap> inst2memstate; //after every store or call, the memory state will change
    std::map<llvm::Constant *, State> const_state;

    std::set<llvm::Function *> skip_function_set;
    std::set<llvm::Function *> work_function_set;
    std::map<llvm::Function *, bool> recursive_flag;
    std::set<llvm::Function *> callstack;
    std::map<llvm::Function *, std::list<FuncSummary>> func_summary;

    OverTaintDetector over_taint_detector;
    LoopVariableSet loop_var;
    std::map<llvm::BasicBlock *, llvm::Value *> loop_controllers;
    std::map<llvm::Value *, llvm::Value *> constraint_var;
    std::set<LeakPoint> protect_set;
    std::set<MemBase *> fake_membase; // fake membase need to be delected when exiting this pass 

    void initialize(llvm::Module &M, llvm::ModuleAnalysisManager & AM);
    // create memory address base and their size; initialize
    
    State &getRegState(Register *,Mode);  // for modify reg2message
    State &getRegStateSeqFinal(Register *); // for read register's state (different from getRegState since
                                          //  when run in spec mode, it may read from seq_seq_final)
    State& getOperandState(llvm::Value *, Mode);

    MemStateMap &getBBIN(llvm::BasicBlock *,Mode);
    MemStateMap &getBBOUT(llvm::BasicBlock *,Mode);

    void runAIonFunction(llvm::Function &, MemStateMap& , Mode);
    bool runAIonBasicBlock(llvm::BasicBlock& , Mode );
    virtual void preProcessBasicBlock( llvm::BasicBlock* );
    virtual void preProcessInstruction( llvm::Instruction* );

    State runonInstruction(llvm::Instruction* , Mode );

    bool isOutOfBound(llvm::Value *ptr);

    State runOnAllocaInst(llvm::AllocaInst *, Mode );
    State runOnCallInst(llvm::CallInst *, Mode );
    State runOnBinaryOperator(llvm::BinaryOperator *, Mode );
    virtual State runOnLoadInst(llvm::LoadInst *, Mode );
    virtual void runOnStoreInst(llvm::StoreInst *, Mode );
    virtual void runOnBranchInst(llvm::BranchInst *, Mode);
    State runOnGetElementPtrInst(llvm::GetElementPtrInst *, Mode);

    State runOnBitCastInst(llvm::BitCastInst *, Mode);
    State runOnPtrToIntInst(llvm::PtrToIntInst *, Mode);
    State runOnIntToPtrInst(llvm::IntToPtrInst *, Mode);

    State runOnPHINode(llvm::PHINode *, Mode);
    State runOnSelectInst(llvm::SelectInst *, Mode);
    State runOnTruncInst(llvm::TruncInst *, Mode);
    State runOnZExtInst(llvm::ZExtInst *, Mode);
    State runOnSExtInst(llvm::SExtInst *, Mode);
    State runOnCmpInst(llvm::CmpInst *, Mode);

    State runOnExtractElementInst(llvm::ExtractElementInst *, Mode);
    State runOnInsertElementInst(llvm::InsertElementInst *, Mode);
    State runOnShuffleVectorInst(llvm::ShuffleVectorInst *, Mode);
    State runOnExtractValueInst(llvm::ExtractValueInst *, Mode);
    State runonFreezeInst(llvm::FreezeInst *, Mode);

    MemStateMap collectRetMemState(llvm::Function &, Mode); // compute a function's ret memstate
    State collectRetState(llvm::Function &, Mode); //compute a function's ret memstate
    MemStateMap& getLatestMemstate(llvm::Instruction *, Mode);
    ValueDomain& getRegValueWithConstraint(Register *, llvm::BasicBlock* );
    void setArgument(llvm::Function *, llvm::CallInst *, Mode); // before runfunction, set argument
    void updateRegState(llvm::Function *); // everytime after a function finnishes its analysis, update the highest place on StateDomain of its instruction's state
    void updateBlockConstraint(llvm::Function *);
    void updateBlockOutMemState(llvm::Function *);
    void setBlockConstraint(llvm::BasicBlock *, std::map<llvm::BasicBlock *, bool> &);

    bool isHighLeakState(State &);

    void PointerCast(State &, llvm::Type *, llvm::Type *); 
};

#endif