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
#include "LightCut.h"
#include "util.h"
#include "SubPass.h"
#include "out.h"

#include <queue>
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

static bool isConstantPointer(Value *V) {
    if (isa<ConstantPointerNull>(V)) return true; // null pointer
    if (isa<GlobalValue>(V)) return true; // global variable/function
    if (isa<ConstantExpr>(V)) return true; // e.g., constant getelementptr
    if (auto *GEP = dyn_cast<GetElementPtrInst>(V))
        return isConstantPointer(GEP->getPointerOperand()); 
    if (auto *BC = dyn_cast<BitCastInst>(V))
        return isConstantPointer(BC->getOperand(0)); 
    return false;
}

PreservedAnalyses LightCutPass::run(Module &M, ModuleAnalysisManager &AM) {

    LightHardeningPass::initialize(M, AM);
    //printModuleInformation(M);

    FunctionAnalysisManager &FAM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

    for (Function &F : M) {
        if (F.isDeclaration()) continue;
        LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
        func2li[&F] = &LI;  
    }

    for(Function &F : M) {
        if (F.isDeclaration()) continue;
        for(auto &bb : F){
            bool flag = false;
            for(auto &inst: bb){
                if(isa<ReturnInst>(&inst)){
                    flag = true;
                    break;
                }
                if(isa<CallInst>(&inst) && is_user_function(dyn_cast<CallInst>(&inst)->getCalledFunction())){
                    flag = true;
                    break;
                }
            }
            if(flag){
                sink_bb.insert(&bb);
            }
        }
    }

    callstack.insert(entry_function);  //add func to callstack
    runAIonFunction(*entry_function, initMem2StateMap, Mode::SEQ);
    // cout << "Debug Count: "<< DEBUG_COUNT << std::endl;
    // cout << "Func Count: "<< FUNC_COUNT << std::endl;


    over_taint_detector = OverTaintDetector();
    callstack.clear();                 // add func to callstack
    callstack.insert(entry_function);  //add func to callstack
    for(auto [func, summary]: func_summary){
        func_summary[func].clear();
    }

    runAIonFunction(*entry_function, initMem2StateMap, Mode::SPEC);
    // cout << "Debug Count: "<< DEBUG_COUNT << std::endl;
    // cout << "Func Count: "<< FUNC_COUNT << std::endl;

    setBlockBB();
    
#ifdef DEBUG_LOG
    // cout << "\n\nLeakPoint: " << endl;
    // for(auto bb : block_bb){
    //     cout << bb << " " << bb->getParent() << endl;
    // }
#endif

    repairLeakPointFence(M, block_bb);

    for(auto val: fake_membase){
        val->deleteValue();
    }
    return PreservedAnalyses::all();
}

void LightCutPass::preProcessBasicBlock(llvm::BasicBlock* bb){
    if(block_bb.find(bb)!=block_bb.end()){
        fence = true;
        affected_bb[bb].clear();
        return;
    }
    for(auto pred: predecessors(bb)){
        affected_bb[bb].insert(affected_bb[pred].begin(),affected_bb[pred].end());
    }
}

void LightCutPass::preProcessInstruction(llvm::Instruction* inst){
    for(int i=0;i<inst->getNumOperands();i++){
        auto op = dyn_cast<Instruction>(inst->getOperand(i));
        if(op==nullptr){
            continue;
        }
        auto it = inst2oobs.find(op);
        if(it!=inst2oobs.end()){
            inst2oobs[inst].insert(it->second.begin(), it->second.end());
        }
    }
}

State LightCutPass::runOnLoadInst(LoadInst* inst, Mode mode) {
    auto ptr = inst->getPointerOperand();
    State& ptr_state = getOperandState(ptr, mode);
    if(mode == Mode::SPEC){
        if((isHighLeakState(ptr_state))){
            block_bb.insert(inst->getParent());
            fence = true;
            return getRegStateSeqFinal(inst);
        }
        if(auto ptr_inst = dyn_cast<Instruction>(ptr)){
            auto it = inst2oobs.find(ptr_inst);
            if(it!=inst2oobs.end() && !it->second.empty()){
                sink_bb.insert(inst->getParent());
            }   
        }
    }
    inst2oobs[inst].insert(affected_bb[inst->getParent()].begin(),affected_bb[inst->getParent()].end());
    
    ValueDomain& ptrvd = ptr_state.vd;
    auto size = getBaseSizeInBits(inst->getType(), DL);
    if(ptrvd.isTop() || !(ptrvd.isPointer())){
        //out of bound
        return State(ValueDomain(Interval(true, size), uInterval(true, size)), TaintDomain(TaintDomain::bitTaintLable::HIGH, size));
    }
    // well-behaved pointer
    State result = State(ValueDomain(Interval(false,size),uInterval(false, size)), TaintDomain(TaintDomain::bitTaintLable::BOT, size));
    set<State *> load_value_set = {&result};
    MemStateMap& current_mem_state = getLatestMemstate(inst, mode);
    bool high_flag = false;
    bool unaligned_read_flag = false;
    for(auto& [membase, pv]: ptrvd.getPointerMap()){
        if(current_mem_state.find(membase) == current_mem_state.end()){
            continue;
        }
        MemoryRegion &mem_region = current_mem_state[membase];
        if(pv.isRegionAligned() && mem_region.isAligned()){
            State& load_state = mem_region.offset2state[getIndex(pv.getPtrGenHistory(), membase->getType())];
            if(inst->getType()->isPointerTy()){
                Type* result_pointer_type = getBaseElementType(dyn_cast<PointerType>(inst->getType())->getPointerElementType());
                Type *load_pointer_type = getPointerType(load_state.vd);
                if(load_pointer_type && !is_same_type(load_pointer_type, result_pointer_type)){
                    State temp = load_state;
                    PointerCast(temp, load_pointer_type, result_pointer_type);
                    result = AbstractDomain::least_upper_bound(result, temp);
                    continue;
                }
            }
            if(size == load_state.getSize() && pv.isCellAligned()){
                load_value_set.insert(&(load_state));
            }else{
                unaligned_read_flag = true;
                if(load_state.td.containHigh()){
                    high_flag = true;
                }
            }
        }else{
            // Overapproxiamate, when load i8 from i64, see it as an unaigned load;
            unaligned_read_flag = true;
            if(mem_region.high_taint){
                high_flag = true;
            }
        }
    }
    State unaligned_read_state;
    if(unaligned_read_flag){
        TaintDomain::bitTaintLable t = high_flag ? TaintDomain::bitTaintLable::HIGH : TaintDomain::bitTaintLable::LOW;
        unaligned_read_state = State(ValueDomain(Interval(true, size), uInterval(true, size)), TaintDomain(t, size));
        load_value_set.insert(&(unaligned_read_state));
    }
    if(load_value_set.empty()){
        return State(ValueDomain(Interval(false, size), uInterval(false, size)), TaintDomain(TaintDomain::bitTaintLable::BOT, size));
    }
    result = AbstractDomain::least_upper_bound(load_value_set);
    if(isHighLeakState(ptr_state)){
        result.td = TaintDomain(TaintDomain::bitTaintLable::HIGH, size);
    }
    return result;
}

void LightCutPass::runOnStoreInst(StoreInst* inst, Mode mode) {
    // an exclusive code to dealt with such case
    // (with my unwilling to modify the structure of PointerValue)
    // when phinode is the pointer value, dealt with phinode's operand 
    auto origin_ptr = inst->getPointerOperand();
    auto val_op =  inst->getValueOperand();
    MemStateMap current_mem_state = getLatestMemstate(inst, mode);
    State& val = getOperandState(val_op, mode);

    set<llvm::Value*> ptr_set;
    if(isa<PHINode>(origin_ptr)){
        PHINode *ptr_phi = dyn_cast<PHINode>(origin_ptr);
        for (int i = 0; i < ptr_phi->getNumIncomingValues();i++){
            auto sub_ptr = ptr_phi->getIncomingValue(i);
            ptr_set.insert(sub_ptr);
        }
    }else{
        ptr_set.insert(origin_ptr);
    }

    for(auto ptr: ptr_set){
        auto size = getBaseSizeInBits(dyn_cast<PointerType>(ptr->getType())->getPointerElementType(), DL);
        if(mode == Mode::SPEC){
            if(getOperandState(ptr, mode).vd.isTop() || point_to_variable_malloc(getOperandState(ptr, mode).vd, mem_map)){
                affected_bb[inst->getParent()].insert(inst->getParent());
                source_bb.insert(inst->getParent());
                return;
            }
            // if(isHighLeakState(getOperandState(ptr, mode))||fence_set.find(inst)!=fence_set.end()){
            //     fence_set.insert(inst);
            //     fence = true;
            //     return;
            // }
        }
        State &ptr_state = getOperandState(ptr, mode);
        ValueDomain &ptrvd = ptr_state.vd;
        for(auto& [membase, pv]: ptrvd.getPointerMap()){
            if(current_mem_state.find(membase) == current_mem_state.end()){
                continue;
            }
            bool bad_store = true;
            auto &mem_region = current_mem_state[membase];
            if(pv.isRegionAligned() && mem_region.isAligned()){
                bad_store = false;
                State &store_state = mem_region.offset2state[getIndex(pv.getPtrGenHistory(), membase->getType())];
                if((size == store_state.getSize()) && pv.isCellAligned()){
                    store_state = AbstractDomain::least_upper_bound(val, store_state);
                }else{// inbound but not aligned
                    auto taintbit = val.td.containHigh() ? TaintDomain::bitTaintLable::HIGH : TaintDomain::bitTaintLable::LOW;
                    auto store_size = store_state.getSize();
                    State temp = State(ValueDomain(Interval(true, store_size), uInterval(true, store_size)), TaintDomain(taintbit, store_size));
                    store_state = AbstractDomain::least_upper_bound(temp, store_state);
                }
                if(isHighLeakState(ptr_state)){
                    store_state.td = TaintDomain(TaintDomain::bitTaintLable::HIGH, store_state.getSize());
                }
            }else if(mem_region.isAligned() && pv.getRange().getLow() == pv.getRange().getHigh() ){
                // a not aligned but precise pointer, find its place
                int offset = pv.getRange().getLow();
                int index = 0;
                Type *current_type = dyn_cast<PointerType>(membase->getType())->getPointerElementType();
                while(offset > 0){
                    if(current_type->isStructTy()){
                        int i = 1;
                        int current_offset = 0;
                        auto *SL = DL->getStructLayout(dyn_cast<StructType>(current_type));
                        for (; i < dyn_cast<StructType>(current_type)->getNumElements();i++){
                            uint cur_size = SL->getElementOffset(i);
                            if(cur_size > offset){
                                break;
                            }
                            current_offset = cur_size;
                            index += getMemoryCellNum(dyn_cast<StructType>(current_type)->getElementType(i - 1));
                        }
                        offset -= current_offset;
                        current_type = dyn_cast<StructType>(current_type)->getElementType(i - 1);
                    }else if(current_type->isArrayTy() || current_type->isVectorTy()){
                        int base_size = DL->getTypeAllocSize(getBaseElementType(current_type));
                        offset = offset % base_size;
                        current_type = getBaseElementType(current_type);
                    }else {
                        break;
                    }
                }
                if(offset == 0){
                    State& store_state = mem_region.offset2state[index];
                    if((size == store_state.getSize()) && pv.isCellAligned()){
                        store_state = AbstractDomain::least_upper_bound(val, store_state);
                    }else{// inbound but not aligned
                        auto taintbit = val.td.containHigh() ? TaintDomain::bitTaintLable::HIGH : TaintDomain::bitTaintLable::LOW;
                        auto store_size = store_state.getSize();
                        State temp = State(ValueDomain(Interval(true, store_size), uInterval(true, store_size)), TaintDomain(taintbit, store_size));
                        store_state = AbstractDomain::least_upper_bound(temp, store_state);
                    }
                    if(isHighLeakState(ptr_state)){
                        store_state.td = TaintDomain(TaintDomain::bitTaintLable::HIGH, store_state.getSize());
                    }
                    bad_store = false;
                }
            }
            if(bad_store){
                mem_region.disableAligned();
                mem_region.high_taint = mem_region.high_taint || val.td.containHigh() || isHighLeakState(ptr_state);
            }
        }
    }
    inst2memstate[inst] = current_mem_state;
}

void LightCutPass::runOnBranchInst(BranchInst* inst, Mode mode){
    if(mode==Mode::SPEC){
        if(inst->isConditional()){
            if(isHighLeakState(getOperandState(inst->getCondition(), mode))){
                // protect_set.insert(LeakPoint(inst->getCondition(), inst->getParent(), LeakType::HighDependentBranch));
                block_bb.insert(inst->getParent());
            }
        }
    }
}

bool LightCutPass::findSinkBB(llvm::BasicBlock* source, llvm::BasicBlock* start_bb){
    std::unordered_set<BasicBlock*> processed;
    std::queue<BasicBlock*> worklist;
    auto LI = func2li[source->getParent()];
    worklist.push(start_bb);
    while(!worklist.empty()){
        auto curbb = worklist.front();
        worklist.pop();
        if(processed.find(curbb)!=processed.end()){
            continue;
        }
        processed.insert(curbb);
        if(LI->getLoopFor(curbb)){
            continue;
        }
        if(sink_bb.find(curbb)!=sink_bb.end() && affected_bb[curbb].find(source)!=affected_bb[curbb].end()){
            return true;
        }
    }
    return false;
}

void LightCutPass::setBlockBB(){

    for(auto bb: source_bb){
        bool find_sink_flag = false;
        // std::cout << "Source: " << bb  <<std::endl;
        auto LI = func2li[bb->getParent()];
        Loop *cur_loop = LI->getLoopFor(bb);
        BasicBlock* cur_bb = bb;
        while (cur_loop) {
            for(auto * loop_bb:cur_loop->getBlocks()){
                if(sink_bb.find(loop_bb)!=sink_bb.end() && affected_bb[loop_bb].find(bb)!=affected_bb[loop_bb].end()){
                    // std::cout << bb  <<std::endl;
                    find_sink_flag = true;
                    break;
                }
            }
            if(find_sink_flag){
                break;
            }
            auto exit_bb = cur_loop->getExitBlock();
            if(exit_bb){
                cur_bb=exit_bb;
            }else{
                break;
            }
            // std::cout << cur_bb <<std::endl;
            cur_loop = cur_loop->getParentLoop();
        }
        if(find_sink_flag){
            block_bb.insert(cur_bb);
        }else if(findSinkBB(bb, cur_bb)){
            block_bb.insert(cur_bb);
        }
    }
    return;
}