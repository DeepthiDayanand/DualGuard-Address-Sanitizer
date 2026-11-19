#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/Casting.h"
#include "llvm/IR/CFG.h"

#include "util.h"
#include "AbstractDomain.h"

#include <iostream>
#include <cassert>
#include <numeric>

using namespace llvm;
using namespace std;

bool is_user_function(const llvm::Function* F) {
    if(F == nullptr){
        return false;
    }
    if(F->getName().find("ERR")!=std::string::npos){
        return false;
    }
    if(F->getName().find("free")!= std::string::npos){
        return false;
    }
    llvm::TargetLibraryInfoImpl TLII;
    llvm::TargetLibraryInfo TLIT(TLII);
    const llvm::TargetLibraryInfo *TLIp = &TLIT; 
    llvm::LibFunc inbuilt_func;
    if(TLIp == 0 || *(uint64_t*)TLIp == 0) return false; 
    return !TLIp->getLibFunc(*F, inbuilt_func)
                && !(F->empty());
}


bool is_memory_allocation_function(const llvm::Function* F){
    if(F==nullptr){
        return false;
    }
    return F->getName() == "_Znam" || F->getName() == "_Znwm" 
        || F->getName().find("malloc")!= std::string::npos
        || F->getName().find("zalloc")!= std::string::npos;
}

bool is_memory_modification_instruction(const Instruction *inst){
    //store and call to non-allocation's user function is regarded as instructions which will modify the memory
    if(isa<StoreInst>(inst)){
        return true;
    }
    if(isa<AllocaInst>(inst)){
        return true;
    }
    if(isa<CallInst>(inst)){
        auto* callinst = dyn_cast<CallInst>(inst);
        Function *callfunc = callinst->getCalledFunction();
        return is_user_function(callfunc) || is_memory_allocation_function(callfunc);
    }
    return false;
}

bool is_assign_instruction(Instruction *inst){
    if((Value*)(inst)->getType()->isIntegerTy() || (Value*)(inst)->getType()->isVectorTy()){
        return true;
    }
    if((Value*)(inst)->getType()->isPointerTy()){
        auto etype = dyn_cast<PointerType>((inst)->getType())->getPointerElementType();
        return !(etype->isFunctionTy());
    }
    return false;
}

bool is_entry_block(BasicBlock*bb){
    return pred_size(bb) == 0;
}

bool is_exit_block(BasicBlock *bb){
    return succ_size(bb) == 0;
}

bool is_conditional_branch_block(BasicBlock* bb){
    if(isa<BranchInst>(bb->getTerminator())){
        return dyn_cast<BranchInst>(bb->getTerminator())->isConditional();
    }
    return false;
}

bool is_same_type(Type* t1, Type* t2){
    if((t1->isIntegerTy() || t1->isDoubleTy()) && (t2->isIntegerTy() || t2->isDoubleTy())){
        return true;
    }
    if(t1->isArrayTy() && t2->isArrayTy()){
        return is_same_type(dyn_cast<ArrayType>(t1)->getElementType(),  dyn_cast<ArrayType>(t2)->getElementType());
    }
    if(t1->isPointerTy() && t2->isPointerTy()){
        return is_same_type(dyn_cast<PointerType>(t1)->getPointerElementType(), dyn_cast<PointerType>(t2)->getPointerElementType());
    }
    if(t1->isVectorTy() && t2->isVectorTy()){
        return is_same_type(dyn_cast<VectorType>(t1)->getPointerElementType(), dyn_cast<VectorType>(t2)->getPointerElementType());
    }
    if(t1->isStructTy() && t2->isStructTy()){
        StructType* st1 = dyn_cast<StructType>(t1);
        StructType *st2 = dyn_cast<StructType>(t2);
        // if(st1->getNumElements()!=st2->getNumElements()){
        //     return false;
        // }
        // for (int i = 0; i < st1->getNumElements();i++){
        //     if(!is_same_type(st1->getElementType(i),st2->getElementType(i))){
        //         return false;
        //     }
        // }
        // return true;
        return st1->isLayoutIdentical(st2);
    }
    return false;
}

bool is_same_base_type(Type *t1, Type *t2){
    if((t1->isIntegerTy() || t1->isDoubleTy()) && (t2->isIntegerTy() || t2->isDoubleTy())){
        return true;
    }
    if(t1->isPointerTy() && t2->isPointerTy()){
        return is_same_type(t1, t2);
    }
    if(t1->isStructTy() && t2->isStructTy()){
        return is_same_type(t1, t2);
    }
    return false;
}

bool is_legal_bitcast(Type *src, Type *dst){
    // from src to dst
    // if dst can "walk from" src, return true
    Type *src_base_type = getBaseElementType(src);
    Type *dst_base_type = getBaseElementType(dst);
    if(src_base_type->isIntegerTy() || src_base_type->isDoubleTy()){
        return dst_base_type->isIntegerTy();
    }else if(src_base_type->isPointerTy()){
        return is_same_type(src_base_type, dst_base_type);
    }else if(src_base_type->isStructTy()){
        if(dst_base_type->isStructTy() && is_same_type(src_base_type, dst_base_type)){ //confused：how to compare two struct
            return true;
        }else{
            return is_legal_bitcast(dyn_cast<StructType>(src_base_type)->getElementType(0), dst_base_type);
        }
    }else{
        std::cerr << "Unexpected Type Opcode\n";
	    std::exit(PassErrorCode::TypeResolveFailure);
    }
}

bool in_same_loop(Instruction* a, Instruction* b, LoopInfo& LI){
    for (auto &Loop : LI) {
        if (Loop->contains(a) && Loop->contains(b)) {
            return true;
        }
    }
    return false;
}

bool is_power_of_two(int64_t n){
    return n > 0 && ((n & (n - 1)) == 0);
}

bool is_satisfiable_constraints(RegConstraintMap & constraints){
    for(auto [reg, vd]:constraints){
        if(vd.isBot()){
            return false;
        }
    }
    return true;
}

bool is_shl_bit_collector(llvm::BinaryOperator *inst){
    if(!(inst->getOpcode() == Instruction::Add)){
        return false;
    }
    auto op0 = inst->getOperand(1);

    if(isa<BinaryOperator>(op0)){
        auto lsh = dyn_cast<BinaryOperator>(op0);
        if(!(lsh->getOpcode() == Instruction::LShr))
        if(isa<ConstantInt>(lsh->getOperand(1))){
            if(getConcreteValue(dyn_cast<ConstantInt>(lsh->getOperand(1)))==1){
                if(isa<PHINode>(lsh->getOperand(0))){
                    auto phi = dyn_cast<PHINode>(lsh->getOperand(0));
                    if(isa<ConstantInt>(phi->getIncomingValue(1))){
                        if(getConcreteValue(dyn_cast<ConstantInt>(phi->getIncomingValue(1)))==0){
                            if(phi->getIncomingValue(0)==inst){
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool is_naive_add_inst(llvm::Instruction * inst){
    if(isa<BinaryOperator>(inst) && inst->getOpcode() == Instruction::Add){
        return true;
    }
    if(isa<GetElementPtrInst>(inst)){
        GetElementPtrInst *ptr_add = dyn_cast<GetElementPtrInst>(inst);
        auto ptrtype = dyn_cast<PointerType>(ptr_add->getOperand(0)->getType())->getPointerElementType();
        if(ptrtype->isIntegerTy()){
            auto inttype = dyn_cast<IntegerType>(ptrtype);
            return (ptr_add->getNumOperands() == 2) && (inttype->getBitWidth() == 8);
        }
    }
    return false;
}

bool point_to_variable_malloc(ValueDomain & vd, BaseAddressMap & mem_map){
    for(auto [membase, pv]:vd.getPointerMap()){
        if(mem_map[membase]==0){
            return true;
        }
    }
    return false;
}

const llvm::DataLayout* getDataLayout(llvm::Value * v){
    if (auto* inst = llvm::dyn_cast<llvm::Instruction>(v)) {
        return &(inst->getParent()->getParent()->getParent()->getDataLayout());
    } else if (auto* arg = llvm::dyn_cast<llvm::Argument>(v)) {
        return &(arg->getParent()->getParent()->getDataLayout());
    } else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(v)) {
        return &(gv->getParent()->getDataLayout());
    }else{
        std::cerr << "TypeResolveFailure" << endl;
        std::exit(PassErrorCode::TypeResolveFailure);
    }
}

llvm::Instruction* getLatestMemstateInst(llvm::Instruction *inst){
    BasicBlock *bb = inst->getParent();
    BasicBlock::reverse_iterator it(inst);
    it++;
    for (; it != bb->rend();it++){
        Instruction *current_inst = &*it;
        if(is_memory_modification_instruction(current_inst)){
            return current_inst;
        }
    }
    return nullptr;
}

Value *getBranchDependence(BranchInst * branchinst){
    if(branchinst->isConditional()){
        return branchinst->getCondition();
    }
    return nullptr;
}

ValueDomain computeConstraint(ValueDomain& origin, CmpInst::Predicate predicate, ValueDomain & vd, uint64_t len){
    // if(origin.getRange().low == origin.getRange().high){
    //     return origin;
    // }
    size_t size = vd.getSize();
    if(origin.isPointer() ^ vd.isPointer()){
        return origin;
    }

    if(predicate == CmpInst::Predicate::ICMP_EQ){
        return AbstractDomain::greatest_lower_bound(origin, vd);
    }else if(predicate == CmpInst::Predicate::ICMP_NE){
        // if(origin.getRange().high <= vd.getRange().high && (indicator == 1)){
        //     return AbstractDomain::upLift(origin, vd, len);
        // }else if(origin.getRange().low >= vd.getRange().low && (indicator == -1)){
        //     return AbstractDomain::downLift(origin, vd, len);
        // }else{
        //     return origin;
        // }
        return origin;
    }else if(CmpInst::isIntPredicate(predicate) || CmpInst::isRelational(predicate)){ 
        ValueDomain result;
        if(predicate == CmpInst::Predicate::ICMP_UGT || predicate == CmpInst::Predicate::ICMP_SGT){
            result = AbstractDomain::downLift(origin, vd, len);
        }else if(predicate == CmpInst::Predicate::ICMP_UGE || predicate == CmpInst::Predicate::ICMP_SGE){
            result = AbstractDomain::downLift(origin, vd, 0);
        }else if(predicate == CmpInst::Predicate::ICMP_ULT || predicate == CmpInst::Predicate::ICMP_SLT){
            result = AbstractDomain::upLift(origin, vd, len);
        }else if(predicate == CmpInst::Predicate::ICMP_ULE || predicate == CmpInst::Predicate::ICMP_SLE){
            result = AbstractDomain::upLift(origin, vd, 0);
        }

        if(CmpInst::isUnsigned(predicate)){
            result.getRange() = u2sInterval(result.getuRange());
        }else{
            result.getuRange() = s2uInterval(result.getRange());
        }
        return result;
    }else{
        std::cerr << "Unexpected Compare Opcode\n";
		std::exit(PassErrorCode::CmpOpResolveFailure);
    }
}


int64_t getConcreteValue(llvm::ConstantInt * constinst){
    return constinst->getZExtValue();
}

BasicBlock *getNextWorkBlock(list<BasicBlock *> & worklist, map<llvm::BasicBlock *, bool> & record){
    // return the first block in list, whose pred have all been run...
    // if no block satisfy such condition, return the first element in list
    for(auto bb: worklist){
        bool flag = true;
        for(auto predbb: predecessors(bb)){
            if(!record[predbb]){
                flag = false;
                break;
            }
        }
        if(flag){
            return bb;
        }
    }
    return worklist.front();
}

Value* getLatestIndicator(Instruction* inst, map<Value*, Value*>& indicator_map){
    BasicBlock *bb = inst->getParent();
    BasicBlock::reverse_iterator it(inst);
    it++;
    for (; it != bb->rend();it++){
        Instruction *current_inst = &*it;
        if(isa<CallInst>(current_inst)){ //&& is_user_function(dyn_cast<CallInst>(current_inst)->getCalledFunction()))){
            auto *calledFunc = dyn_cast<CallInst>(current_inst)->getCalledFunction();
            if (calledFunc && !(calledFunc->isIntrinsic())){
                return indicator_map[current_inst];
            }
        }
    }
    return indicator_map[inst->getParent()];
}

uint getMemoryCellNum(llvm::Type * t){
    // used for initializing MemoryRegion
    if(t->isStructTy()){
        uint num = 0;
        for (int i = 0; i < dyn_cast<StructType>(t)->getNumElements();i++){
            num += getMemoryCellNum(dyn_cast<StructType>(t)->getElementType(i));
        }
        return num;
    }else if(t->isIntegerTy()||t->isDoubleTy()||t->isPointerTy()){
        return 1;
    }else if(t->isArrayTy()){
        return getMemoryCellNum(dyn_cast<ArrayType>(t)->getElementType());
    }else if(t->isVectorTy()){
        return getMemoryCellNum(dyn_cast<VectorType>(t)->getElementType());
    }else{
        std::cerr << "Unexpected Type Opcode\n";
	    std::exit(PassErrorCode::TypeResolveFailure);
    }
}

uint getIndex(std::vector<PtrGen> & ptr_gen, llvm::Type *t){
    // when operating memory accesss, computing coropperrating index in memoryRegeion
    uint num = 0;
    int i = 1;
    t = dyn_cast<PointerType>(t)->getPointerElementType();
    while(i < ptr_gen.size()){
        if(dyn_cast<StructType>(t)){
            // t is struct
            for (int id = 0; id < ptr_gen[i].member;id++){
                num += getMemoryCellNum(dyn_cast<StructType>(t)->getElementType(id));
            }
            t = dyn_cast<StructType>(t)->getElementType(ptr_gen[i].member);
        }else{
            // t is array
            if(t->isArrayTy()){
                t = getBaseElementType(dyn_cast<ArrayType>(t)->getElementType());
            }else if(t->isVectorTy()){
                t = getBaseElementType(dyn_cast<VectorType>(t)->getElementType());
            }
        }
        i++;
    }
    return num;
}

Type* getBaseElementType(Type* t){
    while(1){
        if(t->isArrayTy()){
            t = dyn_cast<ArrayType>(t)->getElementType();
        }else if(isa<FixedVectorType>(t)){
            t = dyn_cast<FixedVectorType>(t)->getElementType();
        }else{
            break;
        }
    }
    return t;
}

llvm::Type *getPointerType(ValueDomain& vd){
    if(vd.isTop() || (vd.getPointerMap().empty())){
        return nullptr;
    }
    for(auto [membase, pv]: vd.getPointerMap()){
        if(pv.isRegionAligned()){
            Type *cur_type = getBaseElementType(dyn_cast<PointerType>(membase->getType())->getPointerElementType());
            for(auto& ptr_gen : pv.getPtrGenHistory()){
                if(ptr_gen.isStruct()){
                    cur_type = dyn_cast<StructType>(cur_type)->getElementType(ptr_gen.member);
                }
            }
            return cur_type;
        }
    }
    return nullptr;
}

uint getBaseSizeInBits(llvm::Type * t, const llvm::DataLayout * DL){
    return DL->getTypeSizeInBits(getBaseElementType(t));
}

uint getFlattenSize(llvm::Type * t,const llvm::DataLayout* DL){
    // return the flattened array/vector
    // for example, [8 * i64] -> 64
    if(t->isIntegerTy()||t->isDoubleTy()){
        return DL->getTypeAllocSize(t);
    }else if(t->isArrayTy()){
        Type* element = dyn_cast<ArrayType>(t)->getElementType();
        return getFlattenSize(element, DL) * dyn_cast<ArrayType>(t)->getNumElements();
    }else if(isa<FixedVectorType>(t)){
        Type* element = dyn_cast<FixedVectorType>(t)->getElementType();
        return getFlattenSize(element, DL) * dyn_cast<FixedVectorType>(t)->getNumElements(); 
    }else if(t->isStructTy()||t->isPointerTy()){
        return 1;
    }else{
        std::cerr << "Unexpected Type Opcode\n";
	    std::exit(PassErrorCode::TypeResolveFailure);
    }
}

llvm::Value *getOriginValue(llvm::Value * v){
    if(isa<Instruction>(v)){
        while(isa<TruncInst>(v) || isa<ZExtInst>(v) || isa<SExtInst>(v)){
            v = dyn_cast<Instruction>(v)->getOperand(0);
        }
    }
    return v;
}

bool computingAddClosure(set<Value *>& valueset, Value * v, Value * dst){
    if(valueset.find(v)!=valueset.end()){
        return false;
    }
    valueset.insert(v);
    if(isa<BinaryOperator>(v)){
        auto inst = dyn_cast<BinaryOperator>(v);
        if (inst->getOpcode() == Instruction::Add || inst->getOpcode() == Instruction::Sub){
            for(int i=0; i<2; i++){
                if(isa<Instruction>(inst->getOperand(i))){
                    auto op = dyn_cast<Instruction>(inst->getOperand(i));
                    if(op == dst){
                        valueset.insert(inst->getOperand(1 - i));
                        return true;
                    }else if(dst == inst->getOperand(1 - i)){
                        continue;
                    }else{
                        set<Value *> appendset = valueset;
                        if(computingAddClosure(appendset, op, dst)){
                            valueset = appendset;
                            return true;
                        }
                    }
                }
            }
        }
    }
    if(isa<GetElementPtrInst>(v)){
        auto inst = dyn_cast<GetElementPtrInst>(v);
        if(inst->getPointerOperand() == dst && inst->getNumOperands() == 2){
            valueset.insert(inst->getOperand(1));
            return true;
        }else{
            return false;
        }
    }
    if(isa<PHINode>(v)){
        auto phi = dyn_cast<PHINode>(v);
        for(int i=0; i<phi->getNumIncomingValues();i++){
            if(phi->getIncomingValue(i) != dst){
                auto op = phi->getIncomingValue(i);
                if(valueset.find(op)!=valueset.end()){
                    continue;
                }
                set<Value *> appendset = valueset;
                if(computingAddClosure(appendset, op, dst)){
                    valueset = appendset;
                    return true;
                }
            }
        }
    }
    return false;
}

void checkAndSetTop(Type* ptr_type, ValueDomain& vd, BaseAddressMap& mem_map, const llvm::DataLayout* DL){
    // check and set top
    if(vd.isTop()){
        return;
    }
    for(auto&[membase, pointer_value]: vd.getPointerMap()){
        if(mem_map[membase] == 0){
            // Assumption: memory allocated by variable will not cause out of bound store in sequential execution ...
            continue;
        }
        if(!pointer_value.isRegionAligned()){
            auto pointer_size = DL->getTypeAllocSize(dyn_cast<PointerType>(membase->getType())->getPointerElementType());
            if((pointer_value.getRange().low < 0) || (pointer_value.getRange().high >= mem_map[membase]*pointer_size) ){
                vd.setTop();
            }
        } else{
            auto pointer_size = getFlattenSize(dyn_cast<PointerType>(ptr_type)->getPointerElementType(), DL);
            auto &ptr_gen = pointer_value.getPtrGenHistory();
            if(ptr_gen[0].range.getHigh() >= mem_map[membase] || ptr_gen[0].range.getLow() < 0){
                vd.setTop();
                break;
            }
            Type* t = membase->getType()->getPointerElementType();
            t = getBaseElementType(t);
            for (int i = 1; i < ptr_gen.size();i++){
                if(t->isIntegerTy() || t->isPointerTy() ||t->isDoubleTy()){
                    if(ptr_gen[i].range.getLow() != 0 || ptr_gen[i].range.getHigh()!=0){
                        vd.setTop();
                        break;
                    }
                }else if(t->isStructTy()){
                    int member = ptr_gen[i].member;
                    if(member<0||member>=dyn_cast<StructType>(t)->getNumElements()){
                        vd.setTop();
                        return;
                    }else{
                        t = dyn_cast<StructType>(t)->getElementType(member);
                    }
                }else if(t->isArrayTy() || isa<FixedVectorType>(t)){
                    uint len = (i == (ptr_gen.size() - 1)) ? pointer_size : 0;
                    if (ptr_gen[i].range.getLow() < 0 || ptr_gen[i].range.getHigh() + len > getFlattenSize(t, DL)){
                        vd.setTop();
                        return;
                    }
                    t = getBaseElementType(t);
                }else{
                    std::cerr << "Unexpected Type Opcode\n";
	                std::exit(PassErrorCode::TypeResolveFailure);
                }
            }
        }
    }
}

void computePredecessorClosure(set<BasicBlock*>& bbset){
    set<BasicBlock *> worklist = bbset;
    while(!worklist.empty()){
        auto workbb = *(worklist.begin());
        for(BasicBlock* predbb : predecessors(workbb)){
            if(bbset.find(predbb) == bbset.end()){
                bbset.insert(predbb);
                worklist.insert(predbb);
            }
        }
        worklist.erase(workbb);
    }
}

TaintDomain getTaintDomain(uint64_t val, TaintDomain::bitTaintLable t, size_t size){
    vector<TaintDomain::bitTaintLable> taint_vec;
    taint_vec.assign(size, TaintDomain::bitTaintLable::ZERO);
    int i = 0;
    while(val != 0){
        taint_vec[i++] = t;
        val = val >> 1;
    }
    return TaintDomain(taint_vec);
}

vector<llvm::Instruction *> getAllLeakPointInBlock(Value *val, BasicBlock *bb, LeakType lp){
    vector<llvm::Instruction *> locations;
    for(auto& inst:*bb){
        if(isa<LoadInst>(&inst) && lp == LeakType::HighDependentMemorryAccessLoad){
            if(dyn_cast<LoadInst>(&inst)->getPointerOperand()==val){
                locations.push_back(&inst);
            }
        }
        if(isa<StoreInst>(inst) && 
            (lp == LeakType::HighDependentMemorryAccessStore || lp == LeakType::OutOfBoundWrite)){
            if(dyn_cast<StoreInst>(&inst)->getPointerOperand()==val){
                locations.push_back(&inst);
            }
        }
        if(isa<BranchInst>(&inst) && dyn_cast<BranchInst>(&inst)->isConditional() && lp ==LeakType::HighDependentBranch){
            if(dyn_cast<BranchInst>(&inst)->getCondition()==val){
                locations.push_back(&inst);
            }
        }
    }
    return locations;
}

void checkTaint(State& s){
    if(s.vd.getRange().low == Interval::getMinLimit(s.vd.getRange().bitWidth) && 
        s.vd.getRange().high == Interval::getMaxLimit(s.vd.getRange().bitWidth)){
            TaintDomain::bitTaintLable t = s.td.containHigh() ? TaintDomain::bitTaintLable::HIGH : TaintDomain::bitTaintLable::LOW;
            s.td = getTaintDomain(s.vd.getuRange().getHigh(), t, s.td.getSize());
        }
}

void statistics(set<Function*>& funcs, stat_info& stats){
    uint load = 0, store = 0, branch = 0, total = 0;
    for(auto func: funcs){
        for(auto& bb:*func){
            for(auto& inst:bb){
                total++;
                if(isa<LoadInst>(&inst)){
                    load++;
                }
                if(isa<StoreInst>(&inst)){
                    store++;
                }
                if(isa<BranchInst>(&inst)){
                    if(dyn_cast<BranchInst>(&inst)->isConditional()){
                        branch++;
                    }
                }
            }
        }
    }
    stats.total_loads = load;
    stats.total_stores = store;
    stats.total_branches = branch;
    stats.total_instructions = total;
    stats.total_funcs = funcs.size();
}

void statistics(std::set<LeakPoint> &leakset, stat_info& stats){
    if(leakset.empty()){
        return;
    }
    for(auto& lp: leakset){
        Value* leakval = lp.val;
        BasicBlock* bb = lp.bb;
        vector<Instruction *> locations = getAllLeakPointInBlock(leakval, bb, lp.type);
        if(lp.type == LeakType::HighDependentMemorryAccessLoad || lp.type == LeakType::HighDependentMemorryAccessStore || lp.type == LeakType::OutOfBoundWrite){
            for (int i = 0; i < locations.size();i++){
                if(isa<LoadInst>(locations[i])){
                    stats.hardened_loads++;
                }else if(isa<StoreInst>(locations[i])){
                    stats.hardened_stores++;
                }
            }
        }else{
            stats.hardened_branches++;
        }
    }
}
