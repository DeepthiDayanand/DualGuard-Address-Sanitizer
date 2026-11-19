#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/Casting.h"

#include "AbstractDomain.h"
#include "util.h"

#include <iostream>
#include <cassert>

using namespace llvm;
using namespace std;



void createMemoryCell(std::vector<State> &state_vec, llvm::Type *t,const llvm::DataLayout* DL){//TODO
    if (t->isIntegerTy() || t->isDoubleTy()) {
        // only poly have double
        auto size = DL->getTypeSizeInBits(t);
        state_vec.push_back(State(size));
    } else if(t->isStructTy()){
        for (int i = 0; i < dyn_cast<StructType>(t)->getNumElements();i++){
            createMemoryCell(state_vec, dyn_cast<StructType>(t)->getElementType(i), DL);
        }
    }else if (t->isPointerTy()) {
        auto size = DL->getTypeSizeInBits(t);
        state_vec.push_back(State(size));
    }else if(t->isArrayTy()){
        createMemoryCell(state_vec, dyn_cast<ArrayType>(t)->getElementType(), DL);
    }else if(t->isVectorTy()){
        createMemoryCell(state_vec, dyn_cast<VectorType>(t)->getElementType(), DL);
    }else{
        std::cerr << "Unexpected Type Opcode\n";
	    std::exit(PassErrorCode::TypeResolveFailure);
    }
    
}

MemoryRegion::MemoryRegion(MemBase * base,const llvm::DataLayout* DL){
    aligned = true;
    llvm::Type *t = base->getType()->getPointerElementType();
    createMemoryCell(offset2state, t, DL);
}

bool State::operator==(const State& other) const{
    return vd == other.vd && td == other.td;
}

bool MemoryRegion::operator==(const MemoryRegion& other) const{
    if(aligned){
        return offset2state == other.offset2state;
    }else{
        return high_taint == other.high_taint;
    }
}

std::ostream& operator<<(std::ostream& os, const State& obj){
    return os << "( ValueDomain: " << obj.vd << "TaintDomain: "
              << obj.td << ") ";
}

template <>
State AbstractDomain::least_upper_bound(State& a, State& b){
    return State(
        AbstractDomain::least_upper_bound(a.vd, b.vd),
        AbstractDomain::least_upper_bound(a.td, b.td));
}

template <>
State AbstractDomain::least_upper_bound(std::set<State *> & stateset){
    size_t size = (*(stateset.begin()))->getSize();
    State result = State(size);
    for(auto* state: stateset){
        result = AbstractDomain::least_upper_bound(result, *state);
    }
    return result;
}

template <>
State AbstractDomain::NOT(State &a){
	return State(
        AbstractDomain::NOT(a.vd),
        AbstractDomain::NOT(a.td));
}

template <>
State AbstractDomain::AND(State& a, State& b){
    return State(
        AbstractDomain::AND(a.vd, b.vd),
        AbstractDomain::AND(a.td, b.td));
}

template <>
State AbstractDomain::OR(State& a, State& b){
    return State(
        AbstractDomain::OR(a.vd, b.vd),
        AbstractDomain::OR(a.td, b.td));
}

template <>
State AbstractDomain::XOR(State& a, State& b){
    return State(
        AbstractDomain::XOR(a.vd, b.vd),
        AbstractDomain::XOR(a.td, b.td));
}

template <>
State AbstractDomain::PLUS(State& a, State& b){
    return State(
        AbstractDomain::PLUS(a.vd, b.vd),
        AbstractDomain::PLUS(a.td, b.td));
}

template <>
State AbstractDomain::MINUS(State& a, State& b){
    return State(
        AbstractDomain::MINUS(a.vd, b.vd),
        AbstractDomain::MINUS(a.td, b.td));
}

template <>
State AbstractDomain::TIME(State& a, State& b){
    return State(
        AbstractDomain::TIME(a.vd, b.vd),
        AbstractDomain::TIME(a.td, b.td));
}

template <>
State AbstractDomain::LSHIFT(State& a, State& b){
    return State(
        AbstractDomain::LSHIFT(a.vd, b.vd),
        AbstractDomain::LSHIFT(a.td, b.td));
}

template <>
State AbstractDomain::LOGIC_RSHIFT(State& a, State& b){
    return State(
        AbstractDomain::LOGIC_RSHIFT(a.vd, b.vd),
        AbstractDomain::LOGIC_RSHIFT(a.td, b.td));
}

template <>
State AbstractDomain::ARITH_RSHIFT(State& a, State& b){
    return State(
        AbstractDomain::ARITH_RSHIFT(a.vd, b.vd),
        AbstractDomain::ARITH_RSHIFT(a.td, b.td));
}

template <>
State AbstractDomain::extract(State& a, uint offset1, uint offset2){
    return State(
        AbstractDomain::extract(a.vd, offset1, offset2),
        AbstractDomain::extract(a.td, offset1, offset2));
}

template <>
State AbstractDomain::concatenation(std::vector<State*>& stateset){
    std::vector<ValueDomain *> vset;
    std::vector<TaintDomain *> tset;
    for (int i = 0; i < stateset.size();i++){
        vset.push_back(&(stateset[i]->vd));
        tset.push_back(&(stateset[i]->td));
    }
    return State(
        AbstractDomain::concatenation(vset),
        AbstractDomain::concatenation(tset));
}

template <>
State AbstractDomain::ZEXT(State& a, uint size){
    return State(
        AbstractDomain::ZEXT(a.vd, size),
        AbstractDomain::ZEXT(a.td, size));
}

template <>
State AbstractDomain::SEXT(State& a, uint size){
    return State(
        AbstractDomain::SEXT(a.vd, size),
        AbstractDomain::SEXT(a.td, size));
}

bool MemoryRegion::containHighLable(){
    if(aligned){
        for(auto state : offset2state){
            if(state.td.containHigh()){
                return true;
            }
        }
        return false;
    } else{
        return high_taint;
    }
}

template <>
MemoryRegion AbstractDomain::least_upper_bound(MemoryRegion &a, MemoryRegion &b){
    if(a.aligned&&b.aligned){
        std::vector<State> statevec;
        for (int i = 0; i < a.offset2state.size();i++){
            // assert: a and b should have same size
            statevec.push_back(AbstractDomain::least_upper_bound(a.offset2state[i], b.offset2state[i]));
        }
        return MemoryRegion(statevec);
    }else{
        return MemoryRegion(a.containHighLable() || b.containHighLable());
    }
}

template <>
MemStateMap AbstractDomain::least_upper_bound(MemStateMap &a, MemStateMap &b){
    MemStateMap result = a;
    for(auto& [membase, memregion]:b){
        if(result.find(membase)==result.end()){
            result[membase] = memregion;
        }else{
            result[membase] = AbstractDomain::least_upper_bound(result[membase], memregion);
        }
    }
    return result;
}

template <>
MemStateMap AbstractDomain::least_upper_bound(set<MemStateMap*>& m2vmapset){
    // computing lub of memory states
    MemStateMap resultset;
    for (MemStateMap *m2smap : m2vmapset){
        resultset = AbstractDomain::least_upper_bound(resultset, *m2smap);
    }
    return resultset;
}

template <>
RegConstraintMap AbstractDomain::least_upper_bound(RegConstraintMap &a, RegConstraintMap &b){
    RegConstraintMap lub = b;
    for(auto&[reg, vd]: a){
        if(b.find(reg)!=b.end()){
            lub[reg] = least_upper_bound(b[reg], vd);
        }else{
            lub[reg] = vd;
        }
    }
    return lub;
}

template <>
RegConstraintMap AbstractDomain::least_upper_bound(set<RegConstraintMap*>& m2vmapset){
    // computing lub of memory states
    RegConstraintMap result;
    bool flag = false;
    for (RegConstraintMap *rvm : m2vmapset){
        if(!flag){
            result = *rvm;
            flag = true;
        }else{
            result = AbstractDomain::least_upper_bound(result, *rvm);
        }
    }
    return result;
}