#include <iostream>
#include <string>

#include "util.h"
#include "LightHardening.h"
#include "AbstractDomain.h"

std::ostream& operator<<(std::ostream& os, const RegConstraintMap& obj) {
    os << "RegConstraintMap:  \n";
    for (auto &kv_pair : obj){
        std::string str;
        llvm::raw_string_ostream rso(str);
        kv_pair.first->printAsOperand(rso);
        os <<"        "<<rso.str() << ": "
           << kv_pair.second<< std::endl;
    }
    return os ;
}


std::ostream& operator<<(std::ostream& os, const BlockConstraintMap& obj) {
    os << "BlockConstraintMap:  (";
    for (auto &kv_pair : obj){
        std::string str;
        llvm::raw_string_ostream rso(str);
        kv_pair.first->printAsOperand(rso, false);    
        os << ": BB" << rso.str() << " "
           << kv_pair.second;
    }
    return os << ")\n";
}

std::ostream& operator<<(std::ostream& os, const std::map<Register*, LightHardeningPass::RegMessage>& obj){
    for(auto &kv_pair: obj){
        std::string str;
        llvm::raw_string_ostream rso(str);
        kv_pair.first->printAsOperand(rso, false);  
        os << "Reg"<<rso.str() << ": "
           << "( state_seq_final : "
           << kv_pair.second.state_seq_final << ") ";
    }
    return os;
}


std::ostream& operator<<(std::ostream& os, const llvm::Value* obj){
    std::string str;
    llvm::raw_string_ostream rso(str);
    if(auto* inst = llvm::dyn_cast<llvm::Instruction>(obj)){
        inst->printAsOperand(rso);
        return os << rso.str();
    }else if(auto* bb = llvm::dyn_cast<llvm::BasicBlock>(obj)){
        bb->printAsOperand(rso);
        return os << rso.str() << " in "<< (std::string)(bb->getParent()->getName());
    }else if(auto* var = llvm::dyn_cast<llvm::GlobalVariable>(obj)){
        return os << (std::string)(var->getName());
    }else if(auto* var = llvm::dyn_cast<llvm::Argument>(obj)){
        var->printAsOperand(rso);
        return os << rso.str();
    }

    return os << (int64_t)obj ;
}

std::ostream& operator<<(std::ostream& os, const LeakPoint& obj) {
    os << "(";
    os << obj.val <<" ("<<(int64_t)(obj.val)<<")"<<", " 
        << obj.bb <<" ("<<(int64_t)(obj.bb)<<")";
    if(obj.type == LeakType::HighDependentMemorryAccessLoad){
        os << " HighDependentMemorryAccessLoad";
    }else if(obj.type == LeakType::HighDependentMemorryAccessStore){
        os << " HighDependentMemorryAccessStore";
    }else if(obj.type == LeakType::HighDependentBranch){
        os << " HighDependentBranch";
    }else{
        os << " OutOfBoundWrite";
    }
    return os << ") ";
}

std::ostream& operator<<(std::ostream& os, const std::set<LeakPoint>& obj) {
    os << "LeakPoint\n";
    for(auto lp:obj){
        os << lp << std::endl;
    }
    return os;
}

std::ostream &operator<<(std::ostream &os, const MemoryRegion &obj){
    if(!obj.aligned){
        return os << "             not aligned: "<<(int64_t)(obj.high_taint)<<std::endl;
    }
    for (int i = 0;i<obj.offset2state.size(); i++){
        os << "             ( " << i << " : " << obj.offset2state[i] << " ), \n";
    }
    return os;
}

std::ostream &operator<<(std::ostream &os, const MemStateMap &obj){
    os << "MemoryState:\n";
    for(auto [membase, mr]: obj){
        os << "        " << membase << " : \n" << mr ;
    }
    return os;
}