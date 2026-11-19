#ifndef UTIL_H
#define UTIL_H

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Analysis/LoopInfo.h"

#include "AbstractDomain.h"

#include <chrono>
#include <unordered_set>
#include <map>
#include <array>
#include <optional>
#include <list>

enum LeakType{
    HighDependentMemorryAccessLoad,
    HighDependentMemorryAccessStore,
    HighDependentBranch,
    OutOfBoundWrite
};

struct LeakPoint{
    llvm::Value *val = nullptr;
    llvm::BasicBlock *bb = nullptr;
    LeakType type;
    LeakPoint(llvm::Value* v, llvm::BasicBlock* b, LeakType t){
        val = v;
        bb = b;
        type = t;
    }
    bool operator==(const LeakPoint& other) const{
        return (val == other.val) && (bb == other.bb);
    };
    bool operator<(const LeakPoint& other) const{
        return (val < other.val) || ((val == other.val) && (bb < other.bb));
    };
};

struct FunctionInState{
    MemStateMap memory_state;
    std::vector<State> argument_state;

    FunctionInState(MemStateMap& ms, std::vector<State>& as){
        memory_state = ms;
        argument_state = as;
    }
    FunctionInState(MemStateMap& ms){
        memory_state = ms;
    }
    bool operator==(const FunctionInState& other) const{
        return (memory_state == other.memory_state) && (argument_state == other.argument_state);
    };
};

struct FunctionOutState{
    MemStateMap memory_state;
    State ret_state;

    FunctionOutState(MemStateMap& ms, State& rs){
        memory_state = ms;
        ret_state = rs;
    }
    FunctionOutState(MemStateMap& ms){
        memory_state = ms;
    }
    bool operator==(const FunctionOutState& other) const{
        return (memory_state == other.memory_state) && (ret_state == other.ret_state);
    };
};

struct FuncSummary{
    FunctionInState in_state;
    FunctionOutState out_state;
    // FuncSummary(FunctionInState& in, FunctionOutState& out){
    //     in_state = in;
    //     out_state = out;
    // }
    bool operator==(const FuncSummary& other) const{
        return (in_state == other.in_state) && (out_state == other.out_state);
    };
};

typedef llvm::Value Register;
typedef std::map<MemBase *, uint64_t> BaseAddressMap;
typedef std::map<Register *, State> RegStateMap;
typedef std::map<Register *, ValueDomain> RegConstraintMap;
typedef std::map<llvm::BasicBlock *, RegConstraintMap> BlockConstraintMap;
typedef std::set<llvm::Value *> loopComputingSet;
typedef std::map<llvm::Value *, loopComputingSet> LoopVariableSet;

bool is_user_function(const llvm::Function* F);
bool is_memory_allocation_function(const llvm::Function* F);
bool is_memory_modification_instruction(const llvm::Instruction *);
bool is_assign_instruction(llvm::Instruction *);
bool is_entry_block(llvm::BasicBlock *);
bool is_exit_block(llvm::BasicBlock *);
bool is_conditional_branch_block(llvm::BasicBlock *);
bool is_same_type(llvm::Type *, llvm::Type *);
bool is_same_base_type(llvm::Type *, llvm::Type *);
bool is_legal_bitcast(llvm::Type *, llvm::Type *);
bool in_same_loop(llvm::Instruction *, llvm::Instruction *, llvm::LoopInfo &);
bool is_power_of_two(int64_t);
bool is_naive_add_inst(llvm::Instruction *);
bool is_satisfiable_constraints(RegConstraintMap &);
bool is_shl_bit_collector(llvm::BinaryOperator *);
bool point_to_variable_malloc(ValueDomain &, BaseAddressMap &);

llvm::Instruction *getLatestMemstateInst(llvm::Instruction *);
llvm::Value *getBranchDependence(llvm::BranchInst *);
ValueDomain computeConstraint(ValueDomain &, llvm::CmpInst::Predicate, ValueDomain &, uint64_t len);
int64_t getConcreteValue(llvm::ConstantInt *);
llvm::BasicBlock *getNextWorkBlock(std::list<llvm::BasicBlock *> &, std::map<llvm::BasicBlock *, bool> &);
llvm::Value *getLatestIndicator(llvm::Instruction *, std::map<llvm::Value *, llvm::Value *> &indicators);

const llvm::DataLayout *getDataLayout(llvm::Value *);
uint getMemoryCellNum(llvm::Type *);
uint getIndex(std::vector<PtrGen> &, llvm::Type *);
llvm::Type *getBaseElementType(llvm::Type *);
llvm::Type *getPointerType(ValueDomain &);
uint getBaseSizeInBits(llvm::Type *, const llvm::DataLayout *);
uint getFlattenSize(llvm::Type *, const llvm::DataLayout *);
llvm::Value *getOriginValue(llvm::Value *); // for trun/zext value, get origin value;
bool computingAddClosure(std::set<llvm::Value *>&, llvm::Value *, llvm::Value *);
TaintDomain getTaintDomain(uint64_t, TaintDomain::bitTaintLable, size_t);

void checkAndSetTop(llvm::Type *, ValueDomain &, BaseAddressMap &, const llvm::DataLayout *);
// check whether this pointer can cause out-of-bound memorry access
// if true, set vd to top(used for approxiamation)

void computePredecessorClosure(std::set<llvm::BasicBlock *> &);
std::vector<llvm::Instruction *> getAllLeakPointInBlock(llvm::Value *, llvm::BasicBlock *, LeakType);

void checkTaint(State& s);

// not every llvm attribution is supported by this code
// when met undefined llvmIR code, this code will exit with ErrorCode
enum PassErrorCode
{
    Success = 0,
    OperandNotFound,              // some OperandCode Type may not be supported
    BlockNotFound,                // can't find block in bb2message
    ConstantResolveFailure,       // some OperandCode Type may not be supported
    BinaryOperatorResolveFailure, // some BinaryOperator Type may not be supported
    CmpOpResolveFailure,          // some CompareOpcode may not be supported
    TypeResolveFailure,           // can't resolve type
    NotConatantMemoryAllocation   // alloc memory_space based on variable
};

struct stat_info{
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time; // start time of processing
    std::chrono::time_point<std::chrono::high_resolution_clock> end_time;   // end time of processing
    int total_funcs = 0; // total number of functions
    int total_instructions = 0; // total number of instructions
    int total_loads = 0; // total number of load instructions
    int total_stores = 0; // total number of store instructions
    int total_branches = 0; // total number of branch instructions
    int processed_funcs_seq = 0; // number of functions processed
    int processed_instructions_seq = 0; // number of instructions processed
    int processed_funcs_spec = 0; // number of functions processed in spec mode
    int processed_instructions_spec = 0; // number of instructions processed in spec mode
    int hardened_loads = 0; // number of loads hardened
    int hardened_stores = 0; // number of stores hardened
    int hardened_branches = 0; // number of branches hardened
};

void statistics(std::set<llvm::Function *> &funcs, stat_info &stats);
void statistics(std::set<LeakPoint> &leakset, stat_info& stats);
#endif