#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Casting.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Pass.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/MDBuilder.h"


#include "AbstractDomain.h"
#include "SubPass.h"
#include "util.h"
#include "out.h"

#include <cstdint>
#include <string>
#include <iostream>
#include <map>
#include <set>

using namespace std;
using namespace llvm;

#define HIGH_INIT 1
#define LOW_INIT 2
#define BOT_INIT 3


set<Function *> collectFunctions(Function * func, set<Function *>& skip_set){
    set<Function *> funcs = {func};
    set<Function *> worklist = {func};
    while(!worklist.empty()){
        auto curfunc = *(worklist.begin());
        for(auto& bb: *curfunc){
            for(auto& inst: bb){
                if(isa<CallInst>(&inst)){
#ifdef DEBUG_LOG
                    errs() << inst << "\n";
#endif
                    auto callfunc = dyn_cast<CallInst>(&inst)->getCalledFunction();
                    if(skip_set.find(callfunc)!=skip_set.end()){
                        continue;
                    }
                    if(is_user_function(callfunc)){
                        if(funcs.find(callfunc)==funcs.end()){
                            funcs.insert(callfunc);
                            worklist.insert(callfunc);
                        }
                    }
                }
            }
        }
        worklist.erase(curfunc);
    }
    return funcs;
}

map<Value *, Value *> collectMemBaseMap(Module &M, set<Function *> &functions, set<MemBase*>& fake_membase){
    map<Value *, Value *> basemap;
    for(auto func: functions){
        for(auto& bb:*func){
            for(auto& inst:bb){
                if(!isa<CallInst>(&inst)){
                    continue;
                }
                auto callinst = dyn_cast<CallInst>(&inst);
                if (!is_memory_allocation_function(callinst->getCalledFunction())){
                    continue; 
                }
                auto arg = callinst->getOperand(0);
                if(!isa<ConstantInt>(arg)){
                    basemap[callinst] = callinst;
                    continue;
                }
                uint total_size = getConcreteValue(dyn_cast<ConstantInt>(arg));
                uint size = 0;
                llvm::Type* t = callinst->getType();
                for (llvm::StructType *sty : M.getIdentifiedStructTypes()){
                    if (!(sty->isSized())){
                        continue;
                    }
                    uint sty_size =  M.getDataLayout().getTypeAllocSize(sty);
                    if((total_size % sty_size == 0) && sty_size > size){
                        size = sty_size;
                        t = sty;
                    }
                }

                if(t == callinst->getType()){
                    basemap[callinst] = callinst;
                }else{
                    PointerType* ptrType = llvm::PointerType::get(t, 0);
                    Instruction *bitcast = CastInst::CreateBitOrPointerCast(callinst, ptrType);
                    fake_membase.insert(bitcast);
                    basemap[callinst] = bitcast;
                }
            }
        }
    }
    return basemap;
}

BaseAddressMap collectMemorySymbol(Module &M, MemStateMap& msm, map<Value *, Value *> &membase_map, set<Function*>& funcs){
    // create a map which maps memory base address to its size(count in bytes) (return value)
    // msm: initialize memoryRigeon
    // membase_map: for malloc, create a symbol with "corresponding type" as base address

    BaseAddressMap memory_map;
    for (GlobalVariable &globalVar : M.globals()) {
        //collect global variables
        if(globalVar.getName().find("annotation")!= std::string::npos){
            continue;
        }
        if(globalVar.getName().find(".str")!= std::string::npos){
            continue;
        }
        if(globalVar.getName().find("__.")!= std::string::npos){
            continue;
        }
        memory_map[&globalVar] = getFlattenSize(dyn_cast<PointerType>(globalVar.getType())->getPointerElementType(), &(M.getDataLayout()));
        msm[(&globalVar)] = MemoryRegion(&globalVar, &(M.getDataLayout()));
        if(globalVar.getName().find("LightHardening")!= std::string::npos){
            string name = "annotation_";
            name.append(globalVar.getName());
            ConstantDataArray *annotation = dyn_cast<ConstantDataArray>(M.getNamedGlobal(name)->getInitializer());
            for (int i = 0; i < annotation->getType()->getNumElements();i++){
                int64_t var = (annotation->getElementAsAPInt(i)).getZExtValue();
                size_t size =  msm[(&globalVar)].offset2state[i].getSize();
                if (var == HIGH_INIT){
                    msm[(&globalVar)].offset2state[i] =  State(ValueDomain(Interval(true, size), uInterval(true, size)), TaintDomain(TaintDomain::bitTaintLable::HIGH, size));
                }else if(var == LOW_INIT){
                    msm[(&globalVar)].offset2state[i] =  State(ValueDomain(Interval(true, size), uInterval(true, size)), TaintDomain(TaintDomain::bitTaintLable::LOW, size));
                }else{// BOT
                    msm[(&globalVar)].offset2state[i] =  State(ValueDomain(size), TaintDomain(TaintDomain::bitTaintLable::BOT, size));
                }
            }

            name = "annotation_variableLen_";
            name.append(globalVar.getName());
            if(M.getNamedGlobal(name)){
                memory_map[&globalVar] = 0;
            }
        }else{
            for (int i = 0; i < msm[(&globalVar)].offset2state.size();i++){
                size_t size =  msm[(&globalVar)].offset2state[i].getSize();
                msm[(&globalVar)].offset2state[i] =  State(ValueDomain(Interval(true, size), uInterval(true, size)), TaintDomain(TaintDomain::bitTaintLable::LOW, size));
            }
        }
    }
    for(Function* func:funcs){
        if(!is_user_function(func)){
            continue;
        }
        for(BasicBlock& bb: *func){
            for (Instruction& inst:bb){
                if(isa<AllocaInst>(&inst)){
                    // memory space allocated by alloca
                    auto allocinst = dyn_cast<AllocaInst>(&inst);
                    uint basesize = getFlattenSize(inst.getType()->getPointerElementType(), &(M.getDataLayout()));
                    uint num = 1;
                    if(allocinst->isArrayAllocation()){
                        if(isa<ConstantInt>(allocinst->getArraySize())){
                            num = getConcreteValue(dyn_cast<ConstantInt>(allocinst->getArraySize()));
                        }else{
                            num = 0;
                        }
                    }
                    memory_map[(Value *)&inst] = basesize * num;
                }
                if(isa<CallInst>(&inst)){ // TODO: malloc's type unknown
                    auto callinst = dyn_cast<CallInst>(&inst);
                    if(is_memory_allocation_function(callinst->getCalledFunction())){
                        auto arg = callinst->getOperand(0);
                        if(isa<ConstantInt>(arg)){
                            size_t total_size = getConcreteValue(dyn_cast<ConstantInt>(arg));
                            Value *truebase = membase_map[&inst];
                            size_t size = M.getDataLayout().getTypeAllocSize(dyn_cast<PointerType>(truebase->getType())->getPointerElementType());
                            uint base = getFlattenSize(dyn_cast<PointerType>(truebase->getType())->getPointerElementType(), &(M.getDataLayout()));
                            memory_map[truebase] = static_cast<unsigned>(base * total_size / size);
                        }else{
                            // cout << "WARNING: memory alloc based on variable at" << &inst << " in " << (string)(func->getName()) << endl;
                            memory_map[(Value *)&inst] = 0; // memory alloc based on variable
                        }
                    }
                }
            }
        }
    }
    return memory_map;
}

LoopVariableSet collectLoopVariable(set<Function*>& funcs){
    LoopVariableSet loop_variable_set;
    for(auto* func:funcs){
        if(!is_user_function(func)){
            continue;
        }
        for(auto& bb:*func){
            for(auto& inst:bb){
                if(isa<PHINode>(&inst)){
                    PHINode *phi = dyn_cast<PHINode>(&inst);
                    if(phi->getNumIncomingValues() > 2){
                        continue;
                    }
                    for (int i = 0; i < phi->getNumIncomingValues();i++){
                        auto val = phi->getIncomingValue(i);
                        if(!isa<Instruction>(val)){
                            continue;
                        }
                        set<Value *> usevals;
                        if(computingAddClosure(usevals, val, phi)){
                            // usevals.insert(phi->getIncomingValue(1 - i));
                            usevals.erase(val);
                            loop_variable_set[val] = usevals;
                        }
                    }
                }
            }
        }
    }
    return loop_variable_set;
}

map<BasicBlock*, Value*> collectBlockControlLoopVariable(set<Function*>& funcs){
    map<BasicBlock*, Value*> loop_controllers;
    for(auto* func:funcs){
        if(!is_user_function(func)){
            continue;
        }
        for(auto& bb:*func){
            for(auto& inst:bb){
                if(isa<PHINode>(&inst)){
                    PHINode *phi = dyn_cast<PHINode>(&inst);
                    if(phi->getNumIncomingValues() == 2){
                        if(isa<ConstantInt>(phi->getIncomingValue(1))){
                            if(getConcreteValue(dyn_cast<ConstantInt>(phi->getIncomingValue(1))) == 0){
                                if(isa<BinaryOperator>(phi->getIncomingValue(0))){
                                    if(dyn_cast<BinaryOperator>(phi->getIncomingValue(0))->getOpcode() == Instruction::Add){
                                        auto addinst = dyn_cast<BinaryOperator>(phi->getIncomingValue(0));
                                        if(isa<ConstantInt>(addinst->getOperand(1))){
                                            if(getConcreteValue(dyn_cast<ConstantInt>(addinst->getOperand(1))) == 1){
                                                if(addinst->getOperand(0)==&inst){
                                                    loop_controllers[addinst->getParent()] = (llvm::Value*)&inst;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return loop_controllers;
}


map<Value *, Value*> collectConstraintVariable(set<Function *> & funcs){
    // when computing constraint, we only compute operand0 (regard operand1 as base)
    map<Value *, Value*> constraint_set;
    for(auto* func:funcs){
        if(!is_user_function(func)){
            continue;
        }
        for(auto& bb:*func){
            for(auto& inst:bb){
                if(isa<BranchInst>(&inst)){
                    auto cond = getBranchDependence(dyn_cast<BranchInst>(&inst));
                    if(cond){
                        if(isa<CmpInst>(cond)){
                            constraint_set[dyn_cast<CmpInst>(cond)->getOperand(0)] = dyn_cast<CmpInst>(cond)->getOperand(1);
                        }
                    }
                }
            }
        }
    }
    return constraint_set;
}

Value *readSpeculativeFlagFromRSP(IRBuilder<> & builder, Instruction * inst){
    builder.SetInsertPoint(inst);
    llvm::InlineAsm *set_rsp_from_ret = InlineAsm::get(
                                            FunctionType::get(builder.getInt64Ty(), false),
                                            "movq %rsp, $0", "=r",true);
    Value *rspValue = builder.CreateCall(set_rsp_from_ret, {});
    Value *new_indicator = builder.CreateAShr(rspValue, llvm::ConstantInt::get(llvm::Type::getInt64Ty(builder.getContext()), 63));
    return new_indicator;
}

void setSpeculativeIndicatorToRSP(IRBuilder<>  &builder, Instruction *inst, Value * indicator){
    builder.SetInsertPoint(inst);
    Value *lshift = builder.CreateShl(indicator, llvm::ConstantInt::get(llvm::Type::getInt64Ty(builder.getContext()), 47));
    llvm::InlineAsm *setrsp = InlineAsm::get(
                                FunctionType::get(builder.getVoidTy(), {builder.getInt64Ty()}, false),
                                "orq ${0}, %rsp", "r,~{flags}",true);
    builder.CreateCall(setrsp, lshift);
}

map<Value *, Value *> setSpeculativeIndicator(Module &M, set<llvm::BasicBlock *> &leakblock){
    IRBuilder<> builder(M.getContext());
    map<Value *, Value *> indicator_map;
    map<Function *, Value*> falseVal;
    for(auto bb :leakblock){
        Value* latest_indicator = nullptr;
        if(is_entry_block(bb)){
            latest_indicator = readSpeculativeFlagFromRSP(builder, bb->getFirstNonPHI());
            indicator_map[bb] = latest_indicator;
            builder.SetInsertPoint(dyn_cast<Instruction>(latest_indicator)->getNextNode());
            InlineAsm *movq = InlineAsm::get(
                        FunctionType::get(builder.getInt64Ty(), false),
                        "movq $$-1, $0", "=r", true);
            falseVal[bb->getParent()] = builder.CreateCall(movq, {});
        }else{
            builder.SetInsertPoint(bb->getFirstNonPHI());
            PHINode* phiNode = builder.CreatePHI(Type::getInt64Ty(bb->getContext()), pred_size(bb));
            indicator_map[bb] = phiNode;
            latest_indicator = phiNode;
        }
        for(auto& inst: *bb){
            if(isa<CallInst>(&inst)){
                auto *calledFunc = dyn_cast<CallInst>(&inst)->getCalledFunction();
                if (calledFunc && !(calledFunc->isIntrinsic())){
                    // set speculative flag for callee
                    setSpeculativeIndicatorToRSP(builder, &inst, latest_indicator);

                    // set speculative flag when return from callee
                    Value *new_indicator = readSpeculativeFlagFromRSP(builder, (&inst)->getNextNode());
                    indicator_map[&inst] = new_indicator;
                    latest_indicator = new_indicator;
                }
            }
        }
        if(is_exit_block(bb)){
            setSpeculativeIndicatorToRSP(builder, bb->getTerminator(), latest_indicator);
        }
    }
    for(auto bb :leakblock){
        auto phiNode = dyn_cast<PHINode>(indicator_map[bb]);
        for(auto predbb : predecessors(dyn_cast<BasicBlock>(bb))){
            if(is_conditional_branch_block(predbb)){
                auto branchinst = dyn_cast<BranchInst>(predbb->getTerminator());
                builder.SetInsertPoint(predbb->getTerminator());
                Value* condition = getBranchDependence(branchinst);
                bool isTrueBranch = (bb == branchinst->getOperand(2)) ? true : false;
                llvm::Value* cmov;
                if(isTrueBranch){
                    if (isa<ConstantInt>(condition)) {
                        cmov = builder.CreateSelect(condition, getLatestIndicator(dyn_cast<Instruction>(branchinst), indicator_map), falseVal[bb->getParent()]);
                    } else {
                        cmov = builder.CreateSelect(condition, getLatestIndicator(dyn_cast<Instruction>(condition), indicator_map), falseVal[bb->getParent()]);
                    }
                }else{
                    if (isa<ConstantInt>(condition)) {
                        cmov = builder.CreateSelect(condition, falseVal[bb->getParent()], getLatestIndicator(dyn_cast<Instruction>(branchinst), indicator_map));
                    } else {
                        cmov = builder.CreateSelect(condition, falseVal[bb->getParent()], getLatestIndicator(dyn_cast<Instruction>(condition), indicator_map));
                    }
                }
                phiNode->addIncoming(cmov, predbb);
            }else{
                phiNode->addIncoming(getLatestIndicator(predbb->getTerminator(), indicator_map), predbb);
            }
        }
    }
    // move cmv to currentbb if it has just one predbb
    for(auto bb :leakblock){
        if(pred_size(bb) == 1){
            BasicBlock *predbb = bb->getSinglePredecessor();
            if(is_conditional_branch_block(predbb)){
                auto phiNode = dyn_cast<PHINode>(indicator_map[bb]);
                auto selectNode = dyn_cast<Instruction>(phiNode->getIncomingValue(0));
                phiNode->replaceAllUsesWith(selectNode);
                phiNode->eraseFromParent();
                selectNode->removeFromParent();
                selectNode->insertBefore(bb->getFirstNonPHI());
                indicator_map[bb] = selectNode;
            }
        }
    }
    return indicator_map;
}

static bool skip_scatter_gather(BasicBlock *BB) {
    std::cout << BB->getName().str() << "\n";
    if (BB->getName().find("PREBUF") != StringRef::npos) {
        std::cout << "Block name contains PREBUGF: " << BB->getName().str() << "\n";
        return true;
    }
    return false;
}


void repairLeakPoint(llvm::Module &M, std::set<LeakPoint> &leakset, std::set<Function*> & funcs){
    if(leakset.empty()){
        return;
    }
    set<BasicBlock *> workbbset;
    for(auto& func:funcs){
        for(auto& bb: *func){
            workbbset.insert(&bb);
        }
    }
    map<Value*, Value*> indicator_map = setSpeculativeIndicator(M, workbbset);
    IRBuilder<> builder(M.getContext());
    for(auto& lp: leakset){
        cout << "Reparing LeakPoint: " << lp << "\n";
        Value* leakval = lp.val;
        BasicBlock* bb = lp.bb;
        vector<Instruction *> locations = getAllLeakPointInBlock(leakval, bb, lp.type);
        if(lp.type == LeakType::HighDependentMemorryAccessLoad || lp.type == LeakType::HighDependentMemorryAccessStore || lp.type == LeakType::OutOfBoundWrite){
            Value *indicator = nullptr, *toint = nullptr, *newval = nullptr, *newptr = nullptr;  // need to be initialized
            for (int i = 0; i < locations.size();i++){
                Value *current_indicator = getLatestIndicator(locations[i], indicator_map);
                if(current_indicator!=indicator){
                    builder.SetInsertPoint(locations[i]);
                    indicator = current_indicator;
                    if(leakval->getType()->isVectorTy()){
                        uint len = dyn_cast<FixedVectorType>(leakval->getType())->getNumElements();
                        Value *vec_indicator = UndefValue::get(FixedVectorType::get(leakval->getType(), len));
                        for (int i = 0; i < len;i++){
                            vec_indicator = builder.CreateInsertElement(vec_indicator, indicator, (uint64_t)i);
                        }
                        indicator = vec_indicator;
                    }
                    toint = builder.CreatePtrToInt(leakval, indicator->getType());
                    newval = builder.CreateOr(toint, indicator);
                    newptr = builder.CreateIntToPtr(newval, leakval->getType());
#ifdef DEBUG_LOG
                    // // cout << toint << " " << newval << " " << newval << endl;
#endif
                }
                if(isa<LoadInst>(locations[i])){
                    LoadInst *load = dyn_cast<LoadInst>(locations[i]);
                    load->setOperand(0, newptr);
                }else if(isa<StoreInst>(locations[i])){
                    StoreInst *store = dyn_cast<StoreInst>(locations[i]);
                    store->setOperand(1, newptr);
                }
            }
        }else{
            Value *indicator = nullptr;
            builder.SetInsertPoint(locations[0]);
            BranchInst *branch = dyn_cast<BranchInst>(locations[0]);
            indicator = getLatestIndicator(locations[0], indicator_map);
            if(getBaseSizeInBits(leakval->getType(), &(M.getDataLayout()))!=64){
                indicator = builder.CreateTrunc(getLatestIndicator(locations[0], indicator_map), getBaseElementType(leakval->getType()));
            }
            Value* newval = builder.CreateOr(branch->getCondition(), indicator);
            branch->setCondition(newval);
        }
    }
}

void repairLeakPointFence(llvm::Module &M, std::set<llvm::Instruction*> &leakset){
    if(leakset.empty()){
        return;
    }
    IRBuilder<> builder(M.getContext());
    for(auto& lp: leakset){
        BasicBlock* bb = lp->getParent();
        builder.SetInsertPoint(lp);
        auto *AsmType = FunctionType::get(Type::getVoidTy(bb->getContext()), false);
        InlineAsm *LFenceAsm = InlineAsm::get(AsmType, "lfence", "", true);
        builder.CreateCall(LFenceAsm);
    }
}

void repairLeakPointFence(llvm::Module &M, std::unordered_set<llvm::BasicBlock*> &leakset){
    if(leakset.empty()){
        return;
    }
    IRBuilder<> builder(M.getContext());
    for(auto& bb: leakset){
        builder.SetInsertPoint(bb->getFirstNonPHI());
        auto *AsmType = FunctionType::get(Type::getVoidTy(bb->getContext()), false);
        InlineAsm *LFenceAsm = InlineAsm::get(AsmType, "lfence", "", true);
        builder.CreateCall(LFenceAsm);
    }
}
