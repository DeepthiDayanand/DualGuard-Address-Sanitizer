#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/CommandLine.h"

#include "LightHardening.h"
#include "util.h"
#include "SubPass.h"
#include "out.h"

#include <utility>
#include <queue>
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <numeric>
#include <regex>

using namespace llvm;
using namespace std;

static cl::opt<bool> Intersection(
    "intersection", 
    cl::desc("Take the intersection between DG analysis and LightSLH analysis"), 
    cl::init(false)
);

static cl::opt<std::string> LogFilePath(
    "dg-log", 
    cl::desc("The log file from DG analysis"), 
    cl::init("")
);

bool resolvePath(std::string &path) {
    SmallString<256> AbsolutePath(path);

    // Get the absolute path
    std::error_code ec = sys::fs::make_absolute(AbsolutePath);
    if (ec || !llvm::sys::fs::exists(AbsolutePath.c_str())) {
        cout << "Error resolving path '" << path << endl;
        return false;
    }

    path = AbsolutePath.str().str();
    return true;
}

PreservedAnalyses LightHardeningPass::run(Module &M, ModuleAnalysisManager &AM) {
#ifdef STAT_INFO
    stat_info stats;
    stats.start_time = std::chrono::high_resolution_clock::now();
#endif
    LightHardeningPass::initialize(M, AM);
    //printModuleInformation(M);
#ifdef STAT_INFO
    statistics(work_function_set, stats);
#endif
    callstack.insert(entry_function);  //add func to callstack
#ifdef DEBUG_LOG
    cout << "Sequential Abstract Interpretation Begin:\n";
#endif
    inst_count = 0;
    func_count = -1;
    runAIonFunction(*entry_function, initMem2StateMap, Mode::SEQ);
#ifdef DEBUG_LOG
    cout << "Sequential Abstract Interpretation Ends.\n";
#endif
#ifdef STAT_INFO
    stats.processed_funcs_seq = func_count;
    stats.processed_instructions_seq = inst_count;
#endif
    over_taint_detector = OverTaintDetector();
    callstack.clear();                 // add func to callstack
    callstack.insert(entry_function);  //add func to callstack
    for(auto [func, summary]: func_summary){
        func_summary[func].clear();
    }

#ifdef DEBUG_LOG
    cout << "\n\nSpeculative Abstract Interpretation Begin:\n";
#endif
    inst_count = 0;
    func_count = -1;
    runAIonFunction(*entry_function, initMem2StateMap, Mode::SPEC);
#ifdef DEBUG_LOG
    cout << "Speculative Abstract Interpretation Ends.\n";
#endif
#ifdef STAT_INFO
    stats.processed_funcs_spec = func_count;
    stats.processed_instructions_spec = inst_count;
#endif
    
#ifdef STAT_INFO
    cout << "\n\nVisited Functions:\n";
    for(auto func : work_function_set){
        cout << func->getName().str() << endl;
    }
    std::set<Function *> unreachableFunctions;
    for (auto &F : M) {
        if (work_function_set.find(&F) == work_function_set.end()) {
            unreachableFunctions.insert(&F);
        }
    }

    cout << "\n\nUnreachable Functions:\n";
    for (auto func : unreachableFunctions) {
        cout << func->getName().str() << endl;
    }

    std::set<BasicBlock *> unsafeBB;
    
    cout << "\n\nLeakPoint: " << endl;
    for(auto leakp : protect_set){
        cout << leakp << endl;
        cout << "\nInstruction: ";
        errs() << *leakp.val;
        errs() << " In function: " << leakp.bb->getParent()->getName() << "\n";
        cout << endl;
        unsafeBB.insert(leakp.bb);
    }

    cout << "\n\nUnsafe Basic Block Size:" << unsafeBB.size() << endl;
    cout << "Tainted GEP Instructions: " << taintedGEPs.size() << endl;
#endif
#ifdef STAT_INFO
    statistics(protect_set, stats);
#endif
    repairLeakPoint(M, protect_set, work_function_set);
#ifdef STAT_INFO
    stats.end_time = std::chrono::high_resolution_clock::now();

    int oobLoad = oobLoads.size();
    int oobStore = oobStores.size();

    std::set<const BasicBlock *> oobBB;
    for (auto inst : oobLoads) {
        oobBB.insert(inst->getParent());
    }
    for (auto inst : oobStores) {
        oobBB.insert(inst->getParent());
    }

    std::cout << "Statistics:\n";
    std::cout << "Processing Time:" << std::chrono::duration_cast<std::chrono::milliseconds>(stats.end_time - stats.start_time).count() / 1000.0 << " s\n";
    std::cout << "Total Functions: " << stats.total_funcs << "\n";
    std::cout << "Total Instructions: " << stats.total_instructions << "\n";
    std::cout << "Total Loads / hardened_loads: " << stats.total_loads << "\\" << stats.hardened_loads << "\n";
    std::cout << "Total Stores / hardened_stores: " << stats.total_stores << "\\" << stats.hardened_stores << "\n";
    std::cout << "Total Branches / hardened_branches: " << stats.total_branches << "\\" << stats.hardened_branches << "\n";
    std::cout << "Processed Functions in SEQ+ SPEC: " << stats.processed_funcs_seq << "+" << stats.processed_funcs_spec<< "\n";
    std::cout << "Processed Instructions in SEQ+ SPEC: " << stats.processed_instructions_seq << "+" << stats.processed_instructions_spec<< "\n";
    std::cout << "Total OOB Loads: " << oobLoad << "\n";
    std::cout << "Total OOB Stores: " << oobStore << "\n";
    std::cout << "Total OOB Memory Access: " << oobLoad + oobStore << "\n";
    std::cout << "OoB Basic Block Size: " << oobBB.size() << "\n";
    std::cout << "Tainted GEP Instructions: " << taintedGEPs.size() << "\n";
    if (Intersection) {
        std::cout << "OOB GEP: " << totalGEPsLightSLH.size() << "\n";
        for (auto &gepInst : totalGEPsLightSLH) {
            std::cout << " -- " << gepInst << "\n";
        }
        std::cout << "OOB GEP after intersection with DG: " << oobGEPs.size() << "\n";
        std::cout << "OOB Ptr (not GEP): " << oobPtrNotGEPs.size() << "\n";
        std::cout << "OOB GEP without debug info: " << oobGEPsWithoutDebug.size() << "\n";
    }
    #endif

    for(auto val: fake_membase){
        val->deleteValue();
    }
    return PreservedAnalyses::all();
}

void LightHardeningPass::initialize(Module &M, llvm::ModuleAnalysisManager & AM){

    // Read DG's results
    if (Intersection) {
        std::ifstream logFile(LogFilePath);
        if (!logFile.is_open()) {
            cout << "Failed to open log file: " << LogFilePath << endl;
            exit(-1);
        }

        std::string line;
        std::regex pattern(R"(Source:\s+(.+)\s+line\s+(\d+))");
        std::smatch match;
        while (std::getline(logFile, line)) {
            if (std::regex_search(line, match, pattern)) {
                std::string fileName = match[1].str();
                bool success = resolvePath(fileName);
                if (!success) {
                    exit(-1);
                }

                int lineNumber = std::stoi(match[2]);
                sourceLines[fileName].insert(lineNumber);
            }
        }

        int totalLines = 0;
        for (auto &fileLinePair : sourceLines) {
            auto &lines = fileLinePair.second;
            for (int line : lines) {
                cout << fileLinePair.first << " at Line " << line << endl;
                totalLines += 1;
            }
        }
        cout << "Total positives by DG: " << totalLines << endl;
    }

    current_module = &M;
    DL = &(current_module->getDataLayout());
    fence = false;

    for(auto &func : M.functions()){
        if(func.getName().find("LightHardeningEntry")!= std::string::npos){
            entry_function = &func;
        }
        string name = "annotation_skip_function_";
        name.append(func.getName());
        if(M.getNamedGlobal(name)){
            skip_function_set.insert(&func);
        }
    }

    work_function_set = collectFunctions(entry_function, skip_function_set);
    for(auto func: work_function_set){
#ifdef DEBUG_LOG
        cout << "Function: " << func->getName().str() << endl;
#endif
        recursive_flag[func] = false;
        func_summary[func] = list<FuncSummary>();
    }

    membase_map = collectMemBaseMap(M, work_function_set, fake_membase);
    mem_map = collectMemorySymbol(M, initMem2StateMap, membase_map, work_function_set);
    loop_var = collectLoopVariable(work_function_set);
    constraint_var = collectConstraintVariable(work_function_set);
    loop_controllers = collectBlockControlLoopVariable(work_function_set);

    over_taint_detector = OverTaintDetector();
    // ConstantArray* taint_annotation;
    // ConstantArray* size_annotation;
    for (GlobalVariable &globalVar : M.globals()) {
        auto size = DL->getTypeSizeInBits(globalVar.getType());
        const_state[&globalVar] = State(ValueDomain((Value *)(&globalVar), size), TaintDomain(TaintDomain::bitTaintLable::LOW, size));
    }
    callstack.clear();
}

State &LightHardeningPass::getRegState(Register *reg ,Mode mode){
    auto it = reg2message.find(reg);
    if(it == reg2message.end()){
        reg2message[reg] = RegMessage(DL->getTypeSizeInBits(getBaseElementType(reg->getType())));
    }
    if(mode == Mode::SEQ){
        return reg2message[reg].state_seq_runtime;
    }else{
        //mode == Mode::SPEC
        return reg2message[reg].state_spec;
    }
}

State &LightHardeningPass::getRegStateSeqFinal(Register *reg){
    auto it = reg2message.find(reg);
    if(it == reg2message.end()){
        tempState = State();
        return tempState;
    }
    return (it->second).state_seq_final;
}

MemStateMap &LightHardeningPass::getBBIN(BasicBlock *bb,Mode mode){
    auto it = bb2message.find(bb);
    if(it == bb2message.end()){
        bb2message[bb] = BasicBlockMessage();
    }
    if(mode == Mode::SEQ){
        return bb2message[bb].block_in_seq;
    }else{
        //mode == Mode::SPEC
        return bb2message[bb].block_in_spec;
    }
}

MemStateMap &LightHardeningPass::getBBOUT(BasicBlock *bb,Mode mode){
    auto it = bb2message.find(bb);
    if(it == bb2message.end()){
        bb2message[bb] = BasicBlockMessage();
    }
    if(mode == Mode::SEQ){
        return bb2message[bb].block_out_seq;
    }else{
        //mode == Mode::SPEC
        return bb2message[bb].block_out_spec;
    }
}

void LightHardeningPass::runAIonFunction(Function &F, MemStateMap& mem2state, Mode mode){
    func_count++;
#ifdef DEBUG_LOG
    cout<<"Enterring Function " << (std::string)F.getName()<<" FUNC_COUNT =" << func_count <<"...\n";  //DEBUG_INFO
    // cout<<"    " << mem2state <<"\n";  //DEBUG_INFO
    // for(unsigned i = 0; i < F.arg_size(); ++i){
    //     cout << "    "<<F.getArg(i) << ": " << getOperandState(F.getArg(i), mode) << endl;
    // }
#endif
    map<BasicBlock*, bool> workrecord;
    for(auto& bb:F){
        // clear every bb information in F
        getBBIN(&bb, mode).clear();
        getBBOUT(&bb, mode).clear();
        workrecord[&bb] = false;
        for(auto& inst: bb){
            if(is_assign_instruction(&inst)){
                // initialize
                if ((mode == Mode::SEQ) || (loop_var.find(&inst) == loop_var.end())){
                    getRegState(&inst, mode) = State(DL->getTypeSizeInBits(getBaseElementType(inst.getType())));
                }
            }
            if(is_memory_modification_instruction(&inst)){
                inst2memstate[&inst].clear();
            }
        }
    }
    getBBIN(&(F.getEntryBlock()), mode) = mem2state;

    list<BasicBlock *> worklist;
    worklist.push_back(&(F.getEntryBlock()));

    while(!worklist.empty()){
        auto workbb = getNextWorkBlock(worklist, workrecord);
#ifdef DEBUG_LOG
        cout <<"    Enterring Block" << workbb <<"..."<< endl;  //DEBUG_INFO
#endif
        worklist.remove(workbb);
        if(workbb != &(F.getEntryBlock())){
            // computing bb_in from its predecessors' out, set block constraint
            set<MemStateMap *> predmem2state;
            for (BasicBlock* predbb : predecessors(workbb)){
                predmem2state.insert(&getBBOUT(predbb, mode));
            }
            getBBIN(workbb, mode) = AbstractDomain::least_upper_bound(predmem2state);
            if (mode == Mode::SEQ){
                setBlockConstraint(workbb, workrecord);
#ifdef DEBUG_LOG
                cout << "      " << workbb << "\'s "<< block_in_constraint[workbb] << endl;
#endif 
                if(!is_satisfiable_constraints(block_in_constraint[workbb])){
                    continue;
                }
            }
        }
        MemStateMap old_out = getBBOUT(workbb, mode);
        // computing based on operational semantics in every block
        // use flag to indicate whether the state of register in the block changes after computation
        bool flag = runAIonBasicBlock(*workbb, mode);
        workrecord[workbb] = true;
        // if the state of block changes, add its successors to worklist
        if(flag || (old_out!=getBBOUT(workbb, mode))){ 
            for (BasicBlock* succbb: successors(workbb)){
                worklist.remove(succbb);
                if (workrecord[succbb] == true){
                    worklist.push_front(succbb);
                }else{
                    worklist.push_back(succbb);
                }
            }
        }
        if(recursive_flag[&F]){
            // add entry block to worklist
            worklist.push_front(&(F.getEntryBlock()));
            recursive_flag[&F] = false;
        }
    }
    if (mode == Mode::SEQ){
        updateRegState(&F);
        updateBlockConstraint(&F);
        updateBlockOutMemState(&F);
    }
#ifdef DEBUG_LOG
    cout <<"Existing Function " << (std::string)F.getName() <<"...\n";  //DEBUG_INFO
#endif
}

bool LightHardeningPass::runAIonBasicBlock(BasicBlock& BB, Mode mode){
#ifdef DEBUG_LOG
    // cout << "      " << &BB << "\'s "<< getBBIN(&BB, mode) << endl;
    cout << "      " <<"Instruction:" << endl;
#endif
    bool flag = false;
    fence = false;
    current_block = &BB;
    preProcessBasicBlock(&BB);
    if(mode == Mode::SEQ){
        seq_reachable_block.insert(&BB);
    }
    if(mode == Mode::SPEC && seq_reachable_block.find(&BB)==seq_reachable_block.end()){
        IRBuilder<> Builder(&BB);
        Builder.SetInsertPoint(BB.getFirstNonPHI());
        auto *AsmType = FunctionType::get(Type::getVoidTy(BB.getContext()), false);
        InlineAsm *LFenceAsm = InlineAsm::get(AsmType, "lfence", "", true);
        Builder.CreateCall(LFenceAsm);

        return false;
    }
    for(Instruction& inst:BB){
        // computing for every instruction
        if(is_assign_instruction(&inst)){
            State old_state = getRegState(&inst, mode);
            getRegState(&inst, mode) = runonInstruction(&inst, mode);
            auto mask = over_taint_detector.acceptNewVal(&inst);
            if(mask.has_value()){
                getRegState(&inst, mode).td.maskLowBit(mask.value());
            }
            // if (mode == Mode::SPEC && getRegState(&inst, mode).td.containHigh() && isa<LoadInst>(&inst)){
            //     oobLoads.insert(dyn_cast<LoadInst>(&inst));
            // }

#ifdef DEBUG_LOG
            cout << "                " << getRegState(&inst, mode) << endl;
#endif
            if(!(old_state==getRegState(&inst, mode))){
                flag = true;
            }
        }else{
            runonInstruction(&inst, mode);
        }
    }
    if(mode == Mode::SPEC && fence){
        getBBOUT(&BB, mode) = bb2message[&BB].block_out_seq_final;
    }else{
        getBBOUT(&BB, mode) = getLatestMemstate(BB.getTerminator(), mode);
    }
    return flag;
}

void LightHardeningPass::preProcessBasicBlock( llvm::BasicBlock* ){
    return;
}

void LightHardeningPass::preProcessInstruction( llvm::Instruction* ){
    return;
}

State LightHardeningPass::runonInstruction(Instruction* inst, Mode mode){
    inst_count++;
    if(inst_count >= 1500000){
        std::cerr << "processing over 1500000 intructions without termination!"<<std::endl;
        exit(1);
    }
#ifdef DEBUG_LOG
    cout << "        dealting with  instruction " << inst << "(with DEBUG_COUNT == "<< inst_count <<")\n"; // DEBUG_INFO
    errs() << *inst << "\n";
#endif
    if(mode == Mode::SPEC && fence && !isa<CallInst>(inst)){
        return getRegStateSeqFinal(inst);
    }
    preProcessInstruction(inst);
    if(isa<AllocaInst>(inst)){
        return runOnAllocaInst(dyn_cast<AllocaInst>(inst), mode);
    } else if(isa<CallInst>(inst)){
        return runOnCallInst(dyn_cast<CallInst>(inst), mode);
    }else if(isa<BinaryOperator>(inst)){
        return runOnBinaryOperator(dyn_cast<BinaryOperator>(inst), mode);
    }else if(isa<LoadInst>(inst)) {
        return runOnLoadInst(dyn_cast<LoadInst>(inst), mode);
    }else if(isa<StoreInst>(inst)){
        runOnStoreInst(dyn_cast<StoreInst>(inst), mode);
    }else if(isa<GetElementPtrInst>(inst)){
        return runOnGetElementPtrInst(dyn_cast<GetElementPtrInst>(inst), mode);
    }else if(isa<BitCastInst>(inst)){
        return runOnBitCastInst(dyn_cast<BitCastInst>(inst), mode);
    }else if(isa<PtrToIntInst>(inst)){
        return runOnPtrToIntInst(dyn_cast<PtrToIntInst>(inst), mode);
    }else if(isa<IntToPtrInst>(inst)){
        return runOnIntToPtrInst(dyn_cast<IntToPtrInst>(inst), mode);
    }else if(isa<SelectInst>(inst)){
        return runOnSelectInst(dyn_cast<SelectInst>(inst), mode);
    }else if(isa<PHINode>(inst)){
        return runOnPHINode(dyn_cast<PHINode>(inst), mode);
    }else if(isa<TruncInst>(inst)){
        return runOnTruncInst(dyn_cast<TruncInst>(inst), mode);
    }else if(isa<ZExtInst>(inst)){
        return runOnZExtInst(dyn_cast<ZExtInst>(inst), mode);
    }else if(isa<SExtInst>(inst)){
        return runOnSExtInst(dyn_cast<SExtInst>(inst), mode);
    }else if(isa<CmpInst>(inst)){
        return runOnCmpInst(dyn_cast<CmpInst>(inst), mode);
    }else if(isa<ExtractElementInst>(inst)){
        return runOnExtractElementInst(dyn_cast<ExtractElementInst>(inst), mode);
    }else if(isa<InsertElementInst>(inst)){
        return runOnInsertElementInst(dyn_cast<InsertElementInst>(inst), mode);
    }else if(isa<ShuffleVectorInst>(inst)){
        return runOnShuffleVectorInst(dyn_cast<ShuffleVectorInst>(inst), mode);
    }else if(isa<FenceInst>(inst)){
        fence = true;
    }else if(isa<ExtractValueInst>(inst)){
        return runOnExtractValueInst(dyn_cast<ExtractValueInst>(inst), mode);
    }else if(isa<FreezeInst>(inst)){
        return runonFreezeInst(dyn_cast<FreezeInst>(inst), mode);
    }else if(isa<BranchInst>(inst)){
        runOnBranchInst(dyn_cast<BranchInst>(inst), mode);
    }
    return State();
}

State LightHardeningPass::runOnAllocaInst(AllocaInst *inst, Mode mode){
    // memory space allocated by alloca
    auto size = DL->getTypeSizeInBits(inst->getType());
    auto ptr_size = DL->getTypeAllocSize(getBaseElementType(dyn_cast<PointerType>(inst->getType())->getPointerElementType()));
    inst2memstate[inst] = getLatestMemstate(inst, mode);
    if(inst2memstate[inst].find(inst) == inst2memstate[inst].end()){
        //no need to initialize
        inst2memstate[inst][inst] = MemoryRegion(inst, DL);
    }
    return State(ValueDomain((Value *)inst, ptr_size), TaintDomain(TaintDomain::bitTaintLable::LOW, size));
}

State LightHardeningPass::runOnCallInst(CallInst *callinst, Mode mode){
    auto* callfunc = callinst->getCalledFunction();
    if(is_memory_allocation_function(callfunc)){
        // alloc function
        llvm::Value *truebase = membase_map[callinst];
        
        auto size = DL->getTypeSizeInBits(callinst->getType());
        auto ptr_size = DL->getTypeAllocSize(getBaseElementType(dyn_cast<PointerType>(truebase->getType())->getPointerElementType()));
        inst2memstate[callinst] = getLatestMemstate(callinst, mode);
        if(inst2memstate[callinst].find(callinst) == inst2memstate[callinst].end()){
        //no need to initialize
            inst2memstate[callinst][truebase] = MemoryRegion(truebase, DL);
        }
        State ret_state;
        auto arg = callinst->getOperand(0);
        if(isa<ConstantInt>(arg)){
            uint total_size = getConcreteValue(dyn_cast<ConstantInt>(arg));
            uint cnt = total_size / ptr_size;
            map<Value *, PointerValue> v2pv;
            PointerValue pv = PointerValue(true, vector<PtrGen>{PtrGen(Interval((int64_t)0, (int64_t)cnt, 64), ptr_size)});
            v2pv[truebase] = pv;
            ret_state = State(ValueDomain(v2pv), TaintDomain(TaintDomain::bitTaintLable::LOW, size));
        }else{
            ret_state =  State(ValueDomain((Value *)truebase, ptr_size), TaintDomain(TaintDomain::bitTaintLable::LOW, size));
        }

        if(callinst!=truebase){
            ret_state.vd.disableRegionAligned();
        }
        return ret_state;
    } else if(is_user_function(callfunc) && (skip_function_set.find(callfunc)==skip_function_set.end())){
        // if func is first called in this context, we set memory state and argument for it
        // otherwise, we see it as a normal basicblock
        // set memorystate
        MemStateMap &caller_memstate = getLatestMemstate(callinst, mode);
        if(callstack.find(callfunc)==callstack.end()){
            // the function is not in the callstack
            /*1. set argument
                2. push function to stack
                3. run abstraction on function
                4. collect memorystate of return
                5. pop function from stack*/ 
            setArgument(callfunc, callinst, mode);
            FunctionInState func_in = FunctionInState(caller_memstate);
            for(unsigned i = 0; i < callinst->getNumOperands() - 1; ++i){
                func_in.argument_state.push_back(getOperandState(callinst->getArgOperand(i), mode));
            }

            for(auto& summary: func_summary[callfunc]){
                if(func_in == summary.in_state){
                // find summary
                    inst2memstate[callinst] = summary.out_state.memory_state;
                    return summary.out_state.ret_state;
                }
            }

            // find no summary
            callstack.insert(callfunc);
            runAIonFunction(*callfunc, caller_memstate, mode);
            callstack.erase(callfunc);
            inst2memstate[callinst] = collectRetMemState(*callfunc, mode); // collect ret memstate
            State ret_state = State();
            if(is_assign_instruction(callinst)){
                ret_state = collectRetState(*callfunc, mode);
            }
            func_summary[callfunc].push_back(FuncSummary{func_in, FunctionOutState(inst2memstate[callinst], ret_state)});
            return ret_state;
        }else{
            /* for callfunc's entry block, compute new entry memstate; if changes, add entryblock to worklist;
                     set ret memstate;*/
            if(caller_memstate != getBBIN(&(callfunc->getEntryBlock()),mode)){
                //set recursive_flag
                recursive_flag[callfunc] = true;
                set<MemStateMap *> entry_mapset = {&(getBBIN(&(callfunc->getEntryBlock()),mode)), &caller_memstate};
                getBBIN(&(callfunc->getEntryBlock()),mode) = AbstractDomain::least_upper_bound(entry_mapset);
            }
            inst2memstate[callinst] = collectRetMemState(*callfunc, Mode::SEQ); // collect ret memstate
        }
        if(is_assign_instruction(callinst)){
            return collectRetState(*callfunc, mode);
        }
    }else{
        if(is_assign_instruction(callinst)){
            // unknown function, set it's valuedomain to top, taindomain to full-high/full-low
            auto size = DL->getTypeSizeInBits(getBaseElementType(callinst->getType()));
            bool high_flag = false;
            for(unsigned i = 0; i < callinst->getNumOperands() - 1; ++i){
                if(getOperandState(callinst->getArgOperand(i), mode).td.containHigh()){
                    high_flag = true;
                    break;
                }
            }
            TaintDomain::bitTaintLable t = high_flag ? TaintDomain::bitTaintLable::HIGH : TaintDomain::bitTaintLable::LOW;
            return State(ValueDomain(Interval(true, size), uInterval(true, size)), TaintDomain(t, size));
        }
    }
    return State();
}

State LightHardeningPass::runOnBinaryOperator(BinaryOperator* inst, Mode mode){
    State state0,state1; //used for record state
    State& src0 = getOperandState(inst->getOperand(0), mode);
    State& src1 = getOperandState(inst->getOperand(1), mode);

    if (inst->getOpcode() == Instruction::Add){
        State retstate =  AbstractDomain::PLUS(src0, src1);
        if(mode==Mode::SEQ && is_shl_bit_collector(inst) 
        && src0.vd.getuRange().high<=1
        && loop_controllers.find(inst->getParent()) != loop_controllers.end()){
            auto maxloop = getOperandState(loop_controllers[inst->getParent()], mode).vd.getuRange().high + 1;
            if(maxloop<64 && !retstate.vd.isTop()){
                retstate.vd.getuRange().high = (1 << maxloop) - 1;
                retstate.vd.getRange().high = (1 << maxloop) - 1;
                retstate.td.maskHighBit((1 << maxloop) - 1);
            }
        }
        return retstate;
    }else if(inst->getOpcode() == Instruction::Sub){
        return AbstractDomain::MINUS(src0, src1);
    }else if(inst->getOpcode() == Instruction::Mul){
        return AbstractDomain::TIME(src0, src1);
    }else if(inst->getOpcode() == Instruction::Shl){
        State retstate =  AbstractDomain::LSHIFT(src0, src1);
        retstate.td.maskHighBit(retstate.vd.getuRange().high); // overtaint
        return retstate;
    }else if(inst->getOpcode() == Instruction::LShr){
        return AbstractDomain::LOGIC_RSHIFT(src0, src1);
    }else if(inst->getOpcode() == Instruction::AShr){
        return AbstractDomain::ARITH_RSHIFT(src0, src1);
    }else if(inst->getOpcode() == Instruction::And){
        return AbstractDomain::AND(src0, src1);
    }else if(inst->getOpcode() == Instruction::Or){
        return AbstractDomain::OR(src0, src1);
    }else if(inst->getOpcode() == Instruction::Xor){
        return AbstractDomain::XOR(src0, src1);
    }else if (inst->getOpcode() == Instruction::UDiv 
          || inst->getOpcode() == Instruction::SDiv){
        bool taint = src0.td.containHigh() || src1.td.containHigh();
        auto size = src0.getSize();
        TaintDomain::bitTaintLable t = taint ? TaintDomain::bitTaintLable::HIGH : TaintDomain::bitTaintLable::LOW;
        return State(ValueDomain(Interval(true, size), uInterval(true, size)), TaintDomain(t, size));
    }else if (inst->getOpcode() == Instruction::URem){
        bool taint = src0.td.containHigh() || src1.td.containHigh();
        auto size = src0.getSize();
        TaintDomain::bitTaintLable t = taint ? TaintDomain::bitTaintLable::HIGH : TaintDomain::bitTaintLable::LOW;
        return State(ValueDomain(u2sInterval(src1.vd.getuRange()), src1.vd.getuRange()), getTaintDomain(src1.vd.getuRange().getHigh(), t, size));
    }else if (inst->getOpcode() == Instruction::SRem){
        bool taint = src0.td.containHigh() || src1.td.containHigh();
        auto size = src0.getSize();
        TaintDomain::bitTaintLable t = taint ? TaintDomain::bitTaintLable::HIGH : TaintDomain::bitTaintLable::LOW;
        uInterval urange = s2uInterval(src1.vd.getRange());
        return State(ValueDomain(src1.vd.getRange(), urange), getTaintDomain(urange.getHigh(), t, size));
    }else{
        std::cerr << "Some BinaryOperator is not supported";
        std::exit(PassErrorCode::BinaryOperatorResolveFailure);
    }
}

bool LightHardeningPass::isOutOfBound(Value *ptr) {
    GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(ptr);
    if (!gep) {
        oobPtrNotGEPs.insert(ptr);
        return true;
    }

    totalGEPsLightSLH.insert(gep);

    const DILocation *loc = gep->getDebugLoc();
    if (!loc) {
        oobGEPsWithoutDebug.insert(gep);
        return true;
    }

    std::string fileName = loc->getFilename().str();
    bool success = resolvePath(fileName);
    int lineNumber = loc->getLine();

    cout << fileName << " at Line " << lineNumber << endl;

    if (!success) {
        exit(-1);
    }

    if (sourceLines.find(fileName) != sourceLines.end() && sourceLines[fileName].find(lineNumber) != sourceLines[fileName].end()) {
        oobGEPs.insert(gep);
        return true;
    }

    return false;
}

State LightHardeningPass::runOnLoadInst(LoadInst* inst, Mode mode){
    auto ptr = inst->getPointerOperand();
    State& ptr_state = getOperandState(ptr, mode);
    if(mode == Mode::SPEC && isHighLeakState(ptr_state)){
        protect_set.insert(LeakPoint(ptr, inst->getParent(), LeakType::HighDependentMemorryAccessLoad));
        // oobLoads.insert(inst);
        return getRegStateSeqFinal(inst);
    }
    ValueDomain& ptrvd = ptr_state.vd;
    auto size = getBaseSizeInBits(inst->getType(), DL);
    if(ptrvd.isTop() || !(ptrvd.isPointer())){
        if(mode == Mode::SPEC){
            oobLoads.insert(inst);
        }
        if (Intersection) {
            if (isOutOfBound(ptr)) {
                // out of bound
                return State(ValueDomain(Interval(true, size), uInterval(true, size)), TaintDomain(TaintDomain::bitTaintLable::HIGH, size));
            } 
        } else {
            // out of bound
            return State(ValueDomain(Interval(true, size), uInterval(true, size)), TaintDomain(TaintDomain::bitTaintLable::HIGH, size));
        }
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
        // if (mode == Mode::SPEC){
        //     oobLoads.insert(inst);
        // }
        result.td = TaintDomain(TaintDomain::bitTaintLable::HIGH, size);
    }
    return result;
}

void LightHardeningPass::runOnStoreInst(StoreInst* inst, Mode mode){
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
        bool need_harden_flag = false;
        if(mode == Mode::SPEC){
            if(isHighLeakState(getOperandState(ptr, mode))){
                protect_set.insert(LeakPoint(origin_ptr, inst->getParent(), LeakType::HighDependentMemorryAccessStore));
                need_harden_flag = true;
            }else if(getOperandState(ptr, mode).vd.isTop() || point_to_variable_malloc(getOperandState(ptr, mode).vd, mem_map)){
                if (mode == Mode::SPEC){
                    oobStores.insert(inst);
                }
                if (Intersection) {
                    if (isOutOfBound(ptr)) {
                        // out of bound
                        protect_set.insert(LeakPoint(origin_ptr, inst->getParent(), LeakType::OutOfBoundWrite));
                        need_harden_flag = true;
                    } 
                } else {
                    // out of bound
                    protect_set.insert(LeakPoint(origin_ptr, inst->getParent(), LeakType::OutOfBoundWrite));
                    need_harden_flag = true;
                }
            }
        }
        State &ptr_state = need_harden_flag ? getRegStateSeqFinal(ptr) : getOperandState(ptr, mode);
        ValueDomain &ptrvd = ptr_state.vd;
        // if(ptrvd.isTop()){
        //     inst2memstate[inst] = current_mem_state;
        //     return; 
        // }
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
                // Overapproxiamate, when load i8 from i64, see it as an unaigned load;
                mem_region.disableAligned();
                mem_region.high_taint = mem_region.high_taint || val.td.containHigh() || isHighLeakState(ptr_state);
            }
#ifdef DEBUG_LOG
            //// cout << "            " << membase << ": " << mem_region;
#endif
        }
    }
    inst2memstate[inst] = current_mem_state;
}

void LightHardeningPass::runOnBranchInst(BranchInst* inst, Mode mode){
    if(mode==Mode::SPEC){
        if(inst->isConditional()){
            if(isHighLeakState(getOperandState(inst->getCondition(), mode))){
                protect_set.insert(LeakPoint(inst->getCondition(), inst->getParent(), LeakType::HighDependentBranch));
            }
        }
    }
}

State LightHardeningPass::runOnGetElementPtrInst(GetElementPtrInst* inst, Mode mode){
    Type *t = inst->getPointerOperand()->getType();
    size_t size = getBaseSizeInBits(inst->getType(), DL);
    State ret_state = getOperandState(inst->getPointerOperand(), mode);
    if(ret_state.vd.isTop()){
        return ret_state;
    }
    for (int i = 1; i < inst->getNumOperands();i++){
        auto operand = inst->getOperand(i);
        State ptr_offset = getOperandState(operand, mode);
        Interval &offset = ptr_offset.vd.getRange();
        TaintDomain &offset_taint = ptr_offset.td;
        if(dyn_cast<Constant>(operand)){
            offset.bitWidth = DL->getTypeAllocSizeInBits(inst->getType());
            offset_taint = AbstractDomain::ZEXT(offset_taint, DL->getTypeAllocSizeInBits(inst->getType()));
        }
        if (i == 1){
            t = dyn_cast<PointerType>(t)->getPointerElementType();
            int64_t len = getFlattenSize(t, DL);
            size_t psize = DL->getTypeAllocSize(t);
            ret_state.vd.addPointer(offset, len, psize);
            auto baseTaint = TaintDomain((int64_t)len, offset.getSize());
            baseTaint = AbstractDomain::TIME(baseTaint, offset_taint);
            ret_state.td = AbstractDomain::PLUS(ret_state.td, baseTaint);
        }else{
            if(t->isStructTy()){
                int64_t val = offset.getLow();
                size_t psize = 0;
                auto *SL = DL->getStructLayout(dyn_cast<StructType>(t));
                psize = SL->getElementOffset(val);
                ret_state.vd.insertPtrGenAction(PtrGen(val, psize), psize);
                t = dyn_cast<StructType>(t)->getElementType(val);
                if(t->isArrayTy() || t->isVectorTy()){
                    int64_t base = getBaseSizeInBits(t, DL);
                    ret_state.vd.insertPtrGenAction(PtrGen(Interval((int64_t)0, size), base), 0);
                }
            }else if(t->isArrayTy()){
                t = dyn_cast<ArrayType>(t)->getElementType();
                int64_t len = getFlattenSize(t, DL);
                size_t psize = DL->getTypeAllocSize(t);
                ret_state.vd.addPointer(offset, len, psize);
                auto baseTaint = TaintDomain((int64_t)len, offset.getSize());
                baseTaint = AbstractDomain::TIME(baseTaint, offset_taint);
                ret_state.td = AbstractDomain::PLUS(ret_state.td, baseTaint);
            }else if(isa<FixedVectorType>(t)){
                t = dyn_cast<FixedVectorType>(t)->getElementType();
                int64_t len = getFlattenSize(t, DL);
                size_t psize = DL->getTypeAllocSize(t);
                ret_state.vd.addPointer(offset, len, psize);
                auto baseTaint = TaintDomain((int64_t)len, offset.getSize());
                baseTaint = AbstractDomain::TIME(baseTaint, offset_taint);
                ret_state.td = AbstractDomain::PLUS(ret_state.td, baseTaint);
            }else{
                std::cerr << "Unexpected Type Opcode\n";
                std::exit(PassErrorCode::TypeResolveFailure);
            }
        }
    }
    for(auto &[membase, pv]: ret_state.vd.getPointerMap()){
        if(mem_map[membase]==0 && pv.getRange().low < 0){
            pv.disableRegionAligned();
            pv.getRange().low = Interval::getMinLimit(pv.getRange().bitWidth);
        }
    }
    if (mode == Mode::SPEC){
        checkAndSetTop(inst->getType(), ret_state.vd, mem_map, DL);
        if (isHighLeakState(ret_state)) {
            taintedGEPs.insert(inst);
        }
    }    
    return ret_state;
}

State LightHardeningPass::runOnPHINode(llvm::PHINode *inst, Mode mode){
    set<State *> stateset;
    for (int i = 0; i < inst->getNumIncomingValues();i++){
        Value *operand = inst->getIncomingValue(i);
        if(inst->getNumIncomingValues() == 2){
            // first time of computing phi
            // set loop_var to its's top
            auto another_comingval = inst->getIncomingValue(1 - i);
            if ((loop_var.find(operand) != loop_var.end()) 
            && (((constraint_var.find(operand) == constraint_var.end()) && (mode == Mode::SEQ)) || (mode == Mode::SPEC))){
                auto size = getOperandState(another_comingval, mode).td.getSize();
                TaintDomain::bitTaintLable t = getOperandState(another_comingval, mode).td.containHigh() ? 
                                            TaintDomain::bitTaintLable::HIGH : TaintDomain::bitTaintLable::LOW;
                if(inst->getType()->isPointerTy()){
                    State result = getOperandState(another_comingval, mode);
                    result.vd.addPointer(Interval(true, POINTER_SIZE_IN_BITS), 1, 1);
                    result.td = TaintDomain(t, size);
                    return result;
                }
                return State(ValueDomain(Interval(true, size), uInterval(true, size)), TaintDomain(t, size));
            }
        }
        stateset.insert(&(getOperandState(inst->getIncomingValue(i), mode)));
    }
    auto ret =  AbstractDomain::least_upper_bound(stateset);
    checkTaint(ret);
    return ret;
}

State LightHardeningPass::runOnSelectInst(llvm::SelectInst* inst, Mode mode){
    State &srcTrue = getOperandState(inst->getTrueValue(), mode);
    State &srcFalse = getOperandState(inst->getFalseValue(), mode);
    State result = AbstractDomain::least_upper_bound(srcTrue, srcFalse);
    if(getOperandState(inst->getCondition(), mode).td.containHigh()){
        result.td = getTaintDomain(result.vd.getuRange().getHigh(), TaintDomain::bitTaintLable::HIGH, result.td.getSize());
    }
    return result;
}

State LightHardeningPass::runOnTruncInst(llvm::TruncInst* inst, Mode mode){
    auto dst_size = getBaseSizeInBits(inst->getDestTy(), DL);
    return AbstractDomain::extract(getOperandState(inst->getOperand(0), mode), 0, dst_size);
}

State LightHardeningPass::runOnZExtInst(llvm::ZExtInst* inst, Mode mode){
    auto dst_size = getBaseSizeInBits(getBaseElementType(inst->getDestTy()), DL);
    return AbstractDomain::ZEXT(getOperandState(inst->getOperand(0), mode), dst_size);
}

State LightHardeningPass::runOnSExtInst(llvm::SExtInst* inst, Mode mode){
    auto dst_size = getBaseSizeInBits(inst->getDestTy(), DL);
    return AbstractDomain::SEXT(getOperandState(inst->getOperand(0), mode), dst_size);
}

State LightHardeningPass::runOnCmpInst(llvm::CmpInst* inst, Mode mode){
    bool high_flag = getOperandState(inst->getOperand(0), mode).td.containHigh() || getOperandState(inst->getOperand(1), mode).td.containHigh();
    TaintDomain::bitTaintLable t = high_flag ? TaintDomain::bitTaintLable::HIGH : TaintDomain::bitTaintLable::LOW;
    return State(ValueDomain(Interval(true, 1), uInterval(true, 1)), TaintDomain(t, 1));
}

void LightHardeningPass::PointerCast(State & result, llvm::Type * src_type, llvm::Type *dst_type){
    auto dst_size = getBaseSizeInBits(dst_type, DL);
    auto src_size = getBaseSizeInBits(src_type, DL);
    src_type = getBaseElementType(dyn_cast<PointerType>(src_type)->getPointerElementType());
    dst_type = getBaseElementType(dyn_cast<PointerType>(dst_type)->getPointerElementType());
    if(src_type->isIntegerTy() && DL->getTypeAllocSize(src_type) == 1){
        //byte level TODO
        for(auto&[membase, pv]: result.vd.getPointerMap()){
            pv.disableRegionAligned();
            if(pv.getRange().low != pv.getRange().high){
                continue;
            }
            Type *t = getBaseElementType(dyn_cast<PointerType>(membase->getType())->getPointerElementType());
            int64_t offset = pv.getRange().low;
            uint size = DL->getTypeAllocSize(getBaseElementType(t));
            int64_t index = offset / size;
            offset = offset % size;
            vector<PtrGen> ptr_record = {PtrGen(Interval(int64_t(index), POINTER_SIZE_IN_BITS), size)};
            while(!is_same_type(t, getBaseElementType(dst_type))){
                if(t->isStructTy()){
                    auto *SL = DL->getStructLayout(dyn_cast<StructType>(t));
                    size = 0;
                    int i = 1;
                    for (; i < dyn_cast<StructType>(t)->getNumElements();i++){
                        uint cursize = SL->getElementOffset(i);
                        if(cursize > offset){
                            break;
                        }
                        size = cursize;
                    }
                    ptr_record.push_back(PtrGen(i - 1, size));
                    offset -= size;
                    t = dyn_cast<StructType>(t)->getElementType(i - 1);
                }else if(t->isArrayTy() || t->isVectorTy()){
                    size = DL->getTypeAllocSize(getBaseElementType(t));
                    index = offset / size;
                    offset = offset % size;
                    ptr_record.push_back(PtrGen(Interval(int64_t(index), POINTER_SIZE_IN_BITS), size));
                    t = getBaseElementType(t);
                }
            }
            if(offset != 0){
                continue;
            }else{
                pv = PointerValue(true, ptr_record);
            }
        }
    }else if(is_legal_bitcast(src_type, dst_type)){
        while(!is_same_base_type(src_type, dst_type)){
            //src_type should be a struct type
            result.vd.insertPtrGenAction(PtrGen(0, 0), 0);
            src_type = dyn_cast<StructType>(src_type)->getElementType(0);
            size_t len = getFlattenSize(src_type, DL);
            result.vd.insertPtrGenAction(PtrGen(Interval((int64_t)0, src_size), len), 0);
            src_type = getBaseElementType(src_type);
        }
        if(src_type->isIntegerTy() && dst_type->isIntegerTy()){
            if(src_size < dst_size){
                // check whether need to disable align
                uint rate = dst_size / src_size;
                for (auto &[membase, pv] : result.vd.getPointerMap()){
                    if(pv.isRegionAligned()){
                        if(pv.isCellAligned()){
                            auto &ptrgen = pv.getLatestPtrGen();
                            if(!(ptrgen.range.getLow() == ptrgen.range.getHigh() && ((ptrgen.range.getLow() % rate)==0))){
                                pv.disableCellAligned();
                            }
                        }
                    }// do nothing when not aligned
                }
            }
        }
    }else if(is_legal_bitcast(dst_type, src_type)){
        // dst_type is a struct, src_type can walk from dst_type
        // dst_ptr_gen should be prefix of src_ptr_gen's
        for (auto &[membase, pv] : result.vd.getPointerMap()){
            if(pv.isRegionAligned()&&pv.isCellAligned()){
                int index = 1;
                Type* t = getBaseElementType(dyn_cast<PointerType>(membase->getType())->getPointerElementType());
                auto& ptr_vec = pv.getPtrGenHistory();
                while(!is_same_base_type(dst_type, t)){
                    // t is struct
                    t = dyn_cast<StructType>(t)->getElementType(ptr_vec[index].member);
                    index++;
                    if(t->isArrayTy() || t->isVectorTy()){
                        index++;
                    }
                }
                // now genptr[0..index] is what dst need
                bool valid = true;
                for (int i = index + 1; i < ptr_vec.size();i++){
                    if(ptr_vec[i].isStruct() && (ptr_vec[i].member!=0)){
                        valid = false;
                        break;
                    }
                    if(!(ptr_vec[i].isStruct())){
                        if((ptr_vec[i].range.getHigh()!=0) || (ptr_vec[i].range.getLow()!=0)){
                            valid = false;
                            break;
                        }
                    }
                }
                if(!valid){
                    pv.disableRegionAligned();
                }else{
                    ptr_vec.erase(ptr_vec.begin() + index, ptr_vec.end());
                }
            }
        }
    }else if(src_type->isPointerTy() && dst_type->isPointerTy()){
        ;
    }else{
        //illegal bit-cast
        result.vd.disableRegionAligned();
    }
}

State LightHardeningPass::runOnBitCastInst(llvm::BitCastInst* inst, Mode mode){
    Type *src_type = inst->getOperand(0)->getType();
    Type *dst_type = inst->getDestTy();
    auto size = getBaseSizeInBits(inst->getType(), DL);
    auto dst_size = getBaseSizeInBits(dst_type, DL);
    auto src_size = getBaseSizeInBits(src_type, DL);
    State result = getOperandState(inst->getOperand(0), mode);
    State temp = result;
    vector<State *> stateset = {&result, &temp};
    if(src_type->isVectorTy() && dst_type->isIntegerTy()){
        for (int i = 1; i < dst_size / src_size;i++){
            result = AbstractDomain::concatenation(stateset);
        }
        return result;
    }
    if(!(src_type->isPointerTy() && dst_type->isPointerTy())){
        // return a top value
        result.vd.disableRegionAligned();
        return AbstractDomain::extract(result, 0, dst_size);
    }
    if(result.vd.isTop()){
        return result;
    }
    // src_type and dsy_type are both pointer, and vd is not top
    PointerCast(result, src_type, dst_type);
    return result;
}

State LightHardeningPass::runOnPtrToIntInst(llvm::PtrToIntInst* inst, Mode mode){
    State result = getOperandState(inst->getOperand(0), mode);
    result.vd.disableRegionAligned();
    return result;
}

State LightHardeningPass::runOnIntToPtrInst(llvm::IntToPtrInst* inst, Mode mode){
    State result =  getOperandState(inst->getOperand(0), mode);
    if(mode==Mode::SPEC){
        checkAndSetTop(inst->getType(), result.vd, mem_map, DL);
    }
    return result;
}

State LightHardeningPass::runOnExtractElementInst(ExtractElementInst* inst, Mode mode){
    return getOperandState(inst->getVectorOperand(), mode);
}

State LightHardeningPass::runOnInsertElementInst(InsertElementInst* inst, Mode mode){
    return AbstractDomain::least_upper_bound(
        getOperandState(inst->getOperand(0), mode),
        getOperandState(inst->getOperand(1), mode) );
}

State LightHardeningPass::runOnShuffleVectorInst(ShuffleVectorInst* inst, Mode mode){
    return AbstractDomain::least_upper_bound(
        getOperandState(inst->getOperand(0), mode),
        getOperandState(inst->getOperand(1), mode) );
}

State LightHardeningPass::runOnExtractValueInst(ExtractValueInst* inst, Mode mode){
    auto size = getBaseSizeInBits(inst->getType(), DL);
    return State(ValueDomain(Interval(true, size), uInterval(true, size)), TaintDomain(TaintDomain::bitTaintLable::HIGH, size));
}

State LightHardeningPass::runonFreezeInst(FreezeInst* inst, Mode mode){
    return getOperandState(inst->getOperand(0), mode);
}

void LightHardeningPass::setArgument(Function * func, CallInst * caller, Mode mode){
    for(unsigned i = 0; i < caller->getNumOperands() - 1; ++i){
        getRegState(func->getArg(i), mode) = getOperandState(caller->getArgOperand(i), mode);
    }
}

MemStateMap LightHardeningPass::collectRetMemState(Function &func, Mode mode){
    set<MemStateMap *> mapset;
    for (auto &bb : func){
        if(is_exit_block(&bb)){
            mapset.insert(&getBBOUT(&bb, mode));
        }
    }
    return AbstractDomain::least_upper_bound(mapset);
}

State LightHardeningPass::collectRetState(Function &func, Mode mode){
    set<State *> stateset;
    for (auto &bb : func){
        if(is_exit_block(&bb)){
            if(isa<ReturnInst>(bb.getTerminator())){
                auto retval = dyn_cast<ReturnInst>(bb.getTerminator())->getOperand(0);
                stateset.insert(&getRegState(retval, mode));
            }
        }
    }
    return AbstractDomain::least_upper_bound(stateset);
}

ValueDomain& LightHardeningPass::getRegValueWithConstraint(Register *reg, BasicBlock* bb){
    if(block_in_constraint[bb].find(reg)!=block_in_constraint[bb].end()){
        return block_in_constraint[bb][reg];
    } else{
        return reg2message[reg].state_seq_runtime.vd;
    }
}

MemStateMap& LightHardeningPass::getLatestMemstate(llvm::Instruction *inst, Mode mode){
    auto lastmeminst = getLatestMemstateInst(inst);
    if(lastmeminst==nullptr){
        return getBBIN(inst->getParent(), mode);
    } else{
        if(inst2memstate.find(lastmeminst)==inst2memstate.end()){
            std::cerr << "can't find MemoryState\n";
            std::exit(PassErrorCode::OperandNotFound);
        }
        return inst2memstate[lastmeminst];
    }
}

void LightHardeningPass::updateRegState(Function * func){
    for(BasicBlock& bb:*func){
        for(auto& inst: bb){
            if(is_assign_instruction(&inst)){
                reg2message[&inst].state_seq_final =
                    AbstractDomain::least_upper_bound(reg2message[&inst].state_seq_final, reg2message[&inst].state_seq_runtime);
            }
        }
    }
}

void LightHardeningPass::updateBlockConstraint(Function * func){
    for(BasicBlock& bb:*func){
        if(block_in_constraint_final.find(&bb)==block_in_constraint_final.end()){
            block_in_constraint_final[&bb] = block_in_constraint[&bb];
        }else{
            block_in_constraint_final[&bb] = AbstractDomain::least_upper_bound(
                block_in_constraint_final[&bb], block_in_constraint[&bb]);
        }
    }
}

void LightHardeningPass::updateBlockOutMemState(Function* func){
    for(BasicBlock& bb:*func){
        bb2message[&bb].block_out_seq_final = AbstractDomain::least_upper_bound(
                bb2message[&bb].block_out_seq_final, bb2message[&bb].block_out_seq);
    }
}

void LightHardeningPass::setBlockConstraint(llvm::BasicBlock * bb, map<BasicBlock*, bool>& record){
    constraint_flag = true;
    if(block_in_constraint.find(bb)==block_in_constraint.end()){
        block_in_constraint[bb] = RegConstraintMap();
    }
    RegConstraintMap result;
    bool flag = true;
    
    for(BasicBlock* pred_bb:predecessors(bb)){
        //compute constraint from predbb
        if(!record[pred_bb]){
            continue;
        }
        current_block = pred_bb;
        if(isa<BranchInst>(pred_bb->getTerminator())){
            RegConstraintMap block_out_cond;
            BranchInst *branchinst = dyn_cast<BranchInst>(pred_bb->getTerminator());
            Value *cond = getBranchDependence(branchinst);
            queue<pair<CmpInst*, CmpInst::Predicate>> condset;
            

            if(cond!=nullptr && isa<SelectInst>(cond)){
                if(bb == branchinst->getOperand(2)){
                    SelectInst *selectinst = dyn_cast<SelectInst>(cond);
                    if (auto *constInt = dyn_cast<ConstantInt>(selectinst->getFalseValue())) {
                        if (constInt->isZero()) {
                            auto sel_cond = selectinst->getCondition();
                            auto trueval = selectinst->getTrueValue();
                            for(llvm::Value * i: {sel_cond, trueval}){
                                if(i!=nullptr && isa<CmpInst>(i)){
                                    condset.push(
                                        make_pair(dyn_cast<CmpInst>(i), dyn_cast<CmpInst>(i)->getPredicate()));
                                }
                            }
                        }
                    }
                }
            }
            if(cond!=nullptr && isa<CmpInst>(cond)){
                CmpInst *cmpinst = dyn_cast<CmpInst>(cond);
                // Confused: getOperand(2) is TrueBlock?
                CmpInst::Predicate pred = (bb == branchinst->getOperand(2)) ? cmpinst->getPredicate()
                                                                            : cmpinst->getInversePredicate();
                condset.push(
                    make_pair(cmpinst, pred));
            }
            block_out_cond = block_in_constraint[pred_bb];
            for(auto& [reg, vd]: block_out_cond){
                if(isa<Instruction>(reg)){
                    auto inst_r = dyn_cast<Instruction>(reg);
                    if(inst_r->getParent()==pred_bb){
                        block_out_cond[reg] = getOperandState(reg, Mode::SEQ).vd;
                    }
                }
            }
            while(!condset.empty()){
                auto cmpinst = condset.front().first;
                auto pred = condset.front().second;
                condset.pop();
                ValueDomain op0constraint, op1constraint;
                Value* cmp_op0 = nullptr, * cmp_op1 = nullptr; 
                bool flag0 =false, flag1 = false;
                // CmpInst *cmpinst = dyn_cast<CmpInst>(cond);
                // // Confused: getOperand(2) is TrueBlock?
                // CmpInst::Predicate pred = (bb == branchinst->getOperand(2)) ? cmpinst->getPredicate()
                //                                                             : cmpinst->getInversePredicate();
                cmp_op0 = cmpinst->getOperand(0);
                cmp_op1 = cmpinst->getOperand(1);
                int64_t len = 1;
                if (!isa<Constant>(cmp_op0)){
                    flag0 = true;
                    if(loop_var.find(cmp_op0)!=loop_var.end() && pred == CmpInst::Predicate::ICMP_NE){
                        set<Value *>& compute_history = loop_var[cmp_op0];
                        int64_t stride = 0;
                        bool stride_flag = true;
                        bool positive = true, negative = true;
                        for(auto loop_compute_var: compute_history){
                            auto &loop_compute_var_range = getOperandState(loop_compute_var, Mode::SEQ).vd.getRange();
                            if(loop_compute_var_range.isBot()){
                                stride_flag = false;
                                positive = false;
                                negative = false;
                                break;
                            }
                            stride = std::gcd(stride, loop_compute_var_range.getHigh());
                            stride = std::gcd(stride, loop_compute_var_range.getLow());
                            if(loop_compute_var_range.getHigh() >= 0 ){
                                negative = false;
                            }
                            if(loop_compute_var_range.getLow() < 0 ){
                                positive = false;
                            }
                        }

                        if(positive && !negative){
                            pred = CmpInst::Predicate::ICMP_SLT;
                        }
                        if(!positive && negative){
                            pred = CmpInst::Predicate::ICMP_SGT;
                        }
                        len = stride_flag ? stride : len;
                    }

                    if(isa<ConstantInt>(cmp_op1)){
                        cmp_op0 = getOriginValue(cmp_op0);
                        auto size = getBaseSizeInBits(cmp_op0->getType(), DL);
                        auto val = getConcreteValue(dyn_cast<ConstantInt>(cmp_op1));
                        ValueDomain temp = ValueDomain(Interval(int64_t(val), size), uInterval(uint64_t(val), size));
                        op0constraint = computeConstraint(getOperandState(cmp_op0, Mode::SEQ).vd, pred, temp, len);
                    }else{
                        op0constraint = computeConstraint(getOperandState(cmp_op0, Mode::SEQ).vd, pred, getOperandState(cmp_op1, Mode::SEQ).vd, len);
                    }
                }
                if(flag0){
                    block_out_cond[cmp_op0] = op0constraint;
                }
                if(flag1){
                    block_out_cond[cmp_op1] = op1constraint;
                }
            }
            if(flag){
                flag = false;
                result = block_out_cond;
            }else{
                result = AbstractDomain::least_upper_bound(result, block_out_cond);
            }
        }
    }
    block_in_constraint[bb] = result;
    constraint_flag = false;
}

State& LightHardeningPass::getOperandState(llvm::Value * v, Mode mode){
    if(isa<Constant>(v)){
        if(isa<ConstantData>(v)){
            if(isa<ConstantInt>(v)){
                if(const_state.find(dyn_cast<Constant>(v))==const_state.end()){
                    auto size = getBaseSizeInBits(v->getType(), DL);
                    auto val = dyn_cast<ConstantInt>(v)->getZExtValue();
                    const_state[dyn_cast<Constant>(v)] = State(ValueDomain((int64_t)val, size), TaintDomain((int64_t)val, size));
                }
                return const_state[dyn_cast<Constant>(v)];
            }else if(isa<ConstantPointerNull>(v)){
                if(const_state.find(dyn_cast<Constant>(v))==const_state.end()){
                    auto size = getBaseSizeInBits(v->getType(), DL);
                    const_state[dyn_cast<Constant>(v)] = State(ValueDomain((int64_t)0, size), TaintDomain((int64_t)0, size));
                }
                return const_state[dyn_cast<Constant>(v)];
            }else if(isa<UndefValue>(v)){
                if(const_state.find(dyn_cast<Constant>(v))==const_state.end()){
                    auto size = getBaseSizeInBits(v->getType(), DL);
                    const_state[dyn_cast<Constant>(v)] = State(size);
                }
                return const_state[dyn_cast<Constant>(v)];
            }else if(isa<ConstantDataVector>(v)){
                auto vec = dyn_cast<ConstantDataVector>(v);
                if(const_state.find(dyn_cast<Constant>(v))==const_state.end()){
                    auto size = getBaseSizeInBits(v->getType(), DL);
                    State result = State(size);
                    for (int i = 0; i < vec->getType()->getNumElements();i++){
                        int64_t var = (vec->getElementAsAPInt(i)).getZExtValue();
                        State temp = State(ValueDomain((int64_t)var, size), TaintDomain((int64_t)var, size));
                        result = AbstractDomain::least_upper_bound(result, temp);
                    }
                    const_state[dyn_cast<Constant>(v)] = result;
                }
                return const_state[dyn_cast<Constant>(v)];
            }else if(isa<ConstantAggregateZero>(v)){
                if(const_state.find(dyn_cast<Constant>(v))==const_state.end()){
                    auto size = getBaseSizeInBits(v->getType(), DL);
                    const_state[dyn_cast<Constant>(v)] = State(ValueDomain((int64_t)0, size), TaintDomain((int64_t)0, size));
                }
                return const_state[dyn_cast<Constant>(v)];
            }else{
                std::cerr << "some Operand Type may not be supported\n";
                std::exit(OperandNotFound);
            }
        }
        else if(isa<GlobalValue>(v)){
            return const_state[dyn_cast<GlobalValue>(v)];
        }else if(isa<ConstantExpr>(v)){
            if(const_state.find(dyn_cast<Constant>(v))==const_state.end()){
                Instruction *inst = dyn_cast<ConstantExpr>(v)->getAsInstruction();
                const_state[dyn_cast<Constant>(v)] = runonInstruction(inst, mode);
                inst->deleteValue();
            }
            return const_state[dyn_cast<Constant>(v)];
        }else if(isa<ConstantAggregate>(v)){
            if(isa<ConstantVector>(v)){
                auto vec = dyn_cast<ConstantVector>(v);
                if(const_state.find(dyn_cast<Constant>(v))==const_state.end()){
                    auto size = getBaseSizeInBits(v->getType(), DL);
                    State result = State(size);
                    for (int i = 0; i < vec->getNumOperands();i++){
                        llvm::Constant *elem = vec->getOperand(i);
                        if(isa<ConstantInt>(elem)){
                            auto intelem = dyn_cast<ConstantInt>(elem);
                            int64_t var = intelem->getZExtValue();
                            State temp = State(ValueDomain((int64_t)var, size), TaintDomain((int64_t)var, size));
                            result = AbstractDomain::least_upper_bound(result, temp);
                        }
                    }
                    const_state[dyn_cast<Constant>(v)] = result;
                }
                return const_state[dyn_cast<Constant>(v)];
            }
        }
        else{
            std::cerr << "some Operand Type may not be supported\n";
            std::exit(OperandNotFound);
        }
    }else{
        if(mode == Mode::SEQ){
            bool constraint_current_block = false;
            if(isa<Instruction>(v)){
                constraint_current_block = (dyn_cast<Instruction>(v)->getParent() == current_block);
            }

            auto &constraint = block_in_constraint[current_block];
            if(constraint.find(v)==constraint.end() || (constraint_flag && constraint_current_block) || loop_var.find(v)==loop_var.end()){
                return getRegState(v, mode);
            }else{
                TaintDomain::bitTaintLable t = getRegState(v, mode).td.containHigh() ? TaintDomain::bitTaintLable::HIGH : TaintDomain::bitTaintLable::LOW;
                tempState = State(constraint[v], getTaintDomain(constraint[v].getuRange().getHigh(), t, getRegState(v, mode).td.getSize()));
                return tempState;
            }
        }else{
            return getRegState(v, mode);
        }
    }
    std::cerr << "some Operand Type may not be supported\n";
    std::exit(OperandNotFound);
}

bool LightHardeningPass::isHighLeakState(State& s){
    return (s.td.get_first_high() >= cache_base) && (s.td.get_first_high() < s.td.getSize());
}

