#include "BaggyBoundsPass.hh"

#include <algorithm>

/* Module pass
 * 1.Adds a new main which initializes baggy bounds  (including stack bottom currently only used to deal with command line args)
 * 2.Saves all globals bounds info
 * 3.Save all heap objects info - replaces malloc,heap etc
 * 4.Replace lib c calls (Needs to be checked)
 */

static cl::opt<bool> ClGlobals(
    "baggy-globals", cl::desc("Save global object bounds"),
    cl::Hidden, cl::init(true));

void BaggyBounds::BaggyBoundsPass::initializeBaggyBoundsAndHandleMain()
{
    Function *originalMain = currentModule->getFunction("main");
    if (!originalMain)
        return;
    // This is supposed to rename and update all uses appropriately
    originalMain->setName("main_original");

    // Create a new main function
    Function *newMain = Function::Create(originalMain->getFunctionType(), originalMain->getLinkage(), "main",
                                         currentModule);

    newMain->copyAttributesFrom(originalMain);

    BasicBlock *entry = BasicBlock::Create(currentModule->getContext(), "entry", newMain);
    IRBuilder<> builder(entry);

    // Create a var which is aligned
    ArrayType *arrayType = ArrayType::get(IntegerType::getInt32Ty(currentModule->getContext()), SLOT_SIZE / 4);

    Value *stackBaseVariable = builder.CreateAlloca(arrayType,
                                                    Constant::getIntegerValue(
                                                        IntegerType::getInt32Ty(
                                                            currentModule->getContext()),
                                                        APInt(32, SLOT_SIZE / 4)),
                                                    "stackBaseVariable");

    Value *stackBottomValue = builder.CreateBitCast(stackBaseVariable, Type::getInt32PtrTy(currentModule->getContext()),
                                                    "stackBaseAddress");

    // Set stack bottom
    Value *baggyStackBottomCallee = (currentModule->getOrInsertFunction("baggy_set_stack_bottom",
                                                                        Type::
                                                                            getVoidTy(
                                                                                currentModule->getContext()), // return type
                                                                        Type::getInt32PtrTy(
                                                                            currentModule->getContext()))
                                         .getCallee());

    builder.CreateCall(dyn_cast<Function>(baggyStackBottomCallee),
                       *(new ArrayRef<Value *>(stackBottomValue)));

    // Call the actual main with args if any
    std::vector<Value *> originalMainFormalArgs;
    // Send whatever arguments received
    for (Function::arg_iterator formalNewMainArg = newMain->arg_begin();
         formalNewMainArg != newMain->arg_end(); ++formalNewMainArg)
    {

        originalMainFormalArgs.push_back(&(*formalNewMainArg));
    }

    Value *originalMainRetValue = builder.CreateCall(dyn_cast<Function>(originalMain)->getFunctionType(),
                                                     originalMain, originalMainFormalArgs);

    // end the basic block with a ret statement
    builder.CreateRet(originalMainRetValue);

    MDNode *MD = MDNode::get(newMain->getContext(), {});
    newMain->setMetadata(BAGGY_SKIP_FUNCTION, MD);

    // Create baggy ctor which does one thing - call baggy init
    Function *baggyCtor = dyn_cast<Function>(currentModule->getOrInsertFunction(BAGGY_CTOR_NAME,
                                                                                Type::getVoidTy(
                                                                                    currentModule->getContext()),
                                                                                NULL)
                                                 .getCallee());
    baggyCtor->addFnAttr(Attribute::NoUnwind);
    if (baggyCtor->empty())
    {

        BasicBlock *entry = BasicBlock::Create(currentModule->getContext(), "entry", baggyCtor);
        IRBuilder<> IRB(entry);
        Function *baggyInit = dyn_cast<Function>(currentModule->getOrInsertFunction("baggy_init",
                                                                                    Type::
                                                                                        getVoidTy(
                                                                                            currentModule->getContext()),
                                                                                    NULL)
                                                     .getCallee());

        IRB.CreateCall(dyn_cast<Function>(baggyInit), None);

        IRB.CreateRetVoid();
        appendToUsed(*currentModule, {baggyCtor});
        appendToGlobalCtors(*currentModule, baggyCtor, 101);
    }
}

void BaggyBounds::BaggyBoundsPass::replaceAllLibraryCalls()
{
    Function *StrCpy = currentModule->getFunction("strcpy");
    if (StrCpy != NULL)
    {
        Value *BaggyStrCpy = currentModule->getOrInsertFunction("baggy_strcpy",
                                                                StrCpy->getFunctionType())
                                 .getCallee();
        StrCpy->replaceAllUsesWith(BaggyStrCpy);
    }

    Function *StrCat = currentModule->getFunction("strcat");
    if (StrCat != NULL)
    {
        Value *BaggyStrCat = currentModule->getOrInsertFunction("baggy_strcat",
                                                                StrCat->getFunctionType())
                                 .getCallee();
        StrCat->replaceAllUsesWith(BaggyStrCat);
    }

    Function *Sprintf = currentModule->getFunction("sprintf");
    if (Sprintf != NULL)
    {
        Value *BaggySprintf = currentModule->getOrInsertFunction("baggy_sprintf",
                                                                 Sprintf->getFunctionType())
                                  .getCallee();
        Sprintf->replaceAllUsesWith(BaggySprintf);
    }

    Function *Snprintf = currentModule->getFunction("snprintf");
    if (Snprintf != NULL)
    {
        Value *BaggySnprintf = currentModule->getOrInsertFunction("baggy_snprintf",
                                                                  Snprintf->getFunctionType())
                                   .getCallee();
        Snprintf->replaceAllUsesWith(BaggySnprintf);
    }
}

void BaggyBounds::BaggyBoundsPass::replaceAllHeapAllocationFunctionCalls()
{
    // Replace all heap related functions like malloc, free, calloc and realloc
    Function *mallocFunc = currentModule->getFunction("malloc");
    if (mallocFunc != NULL)
    {
        Value *baggyMalloc = currentModule->getOrInsertFunction("baggy_malloc",
                                                                mallocFunc->getFunctionType())
                                 .getCallee();
        mallocFunc->replaceAllUsesWith(baggyMalloc);
    }

    Function *freeFunc = currentModule->getFunction("free");
    if (freeFunc != NULL)
    {
        Value *baggyFree = currentModule->getOrInsertFunction("baggy_free",
                                                              freeFunc->getFunctionType())
                               .getCallee();
        freeFunc->replaceAllUsesWith(baggyFree);
    }

    Function *callocFunc = currentModule->getFunction("calloc");
    if (callocFunc != NULL)
    {
        Value *baggyCalloc = currentModule->getOrInsertFunction("baggy_calloc",
                                                                callocFunc->getFunctionType())
                                 .getCallee();
        callocFunc->replaceAllUsesWith(baggyCalloc);
    }

    Function *reallocFunc = currentModule->getFunction("realloc");
    if (reallocFunc != NULL)
    {
        Value *baggyRealloc = currentModule->getOrInsertFunction("baggy_realloc",
                                                                 reallocFunc->getFunctionType())
                                  .getCallee();
        reallocFunc->replaceAllUsesWith(baggyRealloc);
    }

    // Ad hoc way of dealing with new operator, new [] operator (which does not always translate to malloc)
    //  A mature implementation of baggy should leverage interception like ASAN
    //  https://github.com/seahorn/llvm-dsa/blob/master/lib/DSA/AllocatorIdentification.cpp
    Function *newOperator1 = currentModule->getFunction("_Znwm");
    if (newOperator1 != NULL)
    {
        Value *baggyMalloc = currentModule->getOrInsertFunction("baggy_malloc",
                                                                newOperator1->getFunctionType())
                                 .getCallee();
        newOperator1->replaceAllUsesWith(baggyMalloc);
    }

    Function *newOperator2 = currentModule->getFunction("_Znam");
    if (newOperator2 != NULL)
    {
        Value *baggyMalloc = currentModule->getOrInsertFunction("baggy_malloc",
                                                                newOperator2->getFunctionType())
                                 .getCallee();
        newOperator2->replaceAllUsesWith(baggyMalloc);
    }

    Function *freeOperator1 = currentModule->getFunction("_ZdlPv");
    if (freeOperator1 != NULL)
    {
        Value *baggyFree = currentModule->getOrInsertFunction("baggy_free",
                                                              freeOperator1->getFunctionType())
                               .getCallee();
        freeOperator1->replaceAllUsesWith(baggyFree);
    }

    Function *freeOperator2 = currentModule->getFunction("_ZdaPv");
    if (freeOperator2 != NULL)
    {
        Value *baggyFree = currentModule->getOrInsertFunction("baggy_free",
                                                              freeOperator2->getFunctionType())
                               .getCallee();
        freeOperator2->replaceAllUsesWith(baggyFree);
    }

    Function *typeFunc = currentModule->getFunction("__ctype_b_loc");
    if (typeFunc != NULL)
    {
        Value *baggyTypeFunc = currentModule->getOrInsertFunction("baggy___ctype_b_loc",
                                                                  typeFunc->getFunctionType())
                                   .getCallee();
        typeFunc->replaceAllUsesWith(baggyTypeFunc);
    }
}

void BaggyBounds::BaggyBoundsPass::saveGlobalObjectsBounds()
{

    GlobalVariable *globalVariable = NULL;
    bool areThereGlobalObjectsWhoseBoundsNeedToBeSaved = false;

    for (Module::global_iterator iter = currentModule->global_begin();
         iter != currentModule->global_end(); ++iter)
    {
        globalVariable = &*iter;

        if (globalVariable->hasInitializer())
        {
            // TODO- Add a better check
            if (iter->hasName() && (iter->getName().contains("llvm") || iter->getName().contains("gcov")))
                continue;
            areThereGlobalObjectsWhoseBoundsNeedToBeSaved = true;
            break;
        }
    }

    if (!areThereGlobalObjectsWhoseBoundsNeedToBeSaved)
        return;

    Function *baggyGlobalsCtor = dyn_cast<Function>(currentModule->getOrInsertFunction(BAGGY_GLOBALS_CTOR_NAME,
                                                                                       Type::getVoidTy(
                                                                                           currentModule->getContext()),
                                                                                       NULL)
                                                        .getCallee());
    baggyGlobalsCtor->setLinkage(GlobalValue::InternalLinkage);
    // Check why is this done
    baggyGlobalsCtor->addFnAttr(Attribute::NoUnwind);
    // BasicBlock *CtorBB = BasicBlock::Create(currentModule->getContext(), "", baggyGlobalsCtor);
    // ReturnInst::Create(currentModule->getContext(), CtorBB);
    //    // Ensure Ctor cannot be discarded, even if in a comdat. (NS - How this works)
    appendToUsed(*currentModule, {baggyGlobalsCtor});

    // Create baggy globals ctor which  saves global objects bounds
    if (baggyGlobalsCtor)
    {
        BasicBlock *entry = BasicBlock::Create(currentModule->getContext(), "entry", baggyGlobalsCtor);
        IRBuilder<> IRB(entry);
        Function *baggyInit = dyn_cast<Function>(currentModule->getOrInsertFunction("baggy_init",
                                                                                    Type::
                                                                                        getVoidTy(
                                                                                            currentModule->getContext()),
                                                                                    NULL)
                                                     .getCallee());

        IRB.CreateCall(dyn_cast<Function>(baggyInit), None);

        // IRBuilder<> IRB(baggyGlobalsCtor->getEntryBlock().getTerminator());

        //  Loop through global variables
        for (Module::global_iterator iter = currentModule->global_begin();
             iter != currentModule->global_end(); ++iter)
        {
            globalVariable = &*iter;

            if (globalVariable->hasInitializer())
            {
                // TODO- Add a better check
                if (iter->hasName() && (iter->getName().contains("llvm") || iter->getName().contains("gcov")))
                    continue;
                unsigned int allocation_size = DL->getTypeAllocSize(globalVariable->getValueType());
                // Set the alignment on the global variable
                unsigned int real_allocation_size = get_alignment(allocation_size);
                iter->setAlignment(MaybeAlign(std::max<uint64_t>(iter->getAlignment(), real_allocation_size)));

                // Add an instruction to update the size_table with the global variable's size
                Constant *location = ConstantExpr::getCast(Instruction::PtrToInt, dyn_cast<Constant>(iter),
                                                           IntegerType::get(currentModule->getContext(), 64));
                IRB.Insert(get_save_in_table_instr(*currentModule, location, real_allocation_size));
            }
        }
        IRB.CreateRetVoid();

        appendToGlobalCtors(*currentModule, baggyGlobalsCtor, 102);
    }
}

void BaggyBounds::BaggyBoundsPass::getAnalysisUsage(AnalysisUsage &AU)
{
}

bool BaggyBounds::BaggyBoundsPass::runOnModule(Module &m)
{
    for (Module::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Function *func = &*iter;
        for (auto &name : skippedFunctions)
        {
            if (func->hasName() && func->getName().startswith(name))
            {
                MDNode *MD = MDNode::get(func->getContext(), {});
                func->setMetadata(BAGGY_SKIP_FUNCTION, MD);
                break;
            }
        }
    }

    DL = const_cast<DataLayout *>(&(m.getDataLayout()));
    currentModule = &m;

    initializeBaggyBoundsAndHandleMain();

    if (ClGlobals)
        saveGlobalObjectsBounds();

    if (!HEAP_PROTECTION_OFF)
    {
        // TODO - Check if these wrappers are correct
        //        replaceAllLibraryCalls();
        replaceAllHeapAllocationFunctionCalls();
    }

    return true;
}

char BaggyBounds::BaggyBoundsPass::ID = 0;
std::set<std::string> BaggyBounds::BaggyBoundsPass::skippedFunctions = {
    /* namd */
    "_ZN20ComputeNonbondedUtil16calc_self_energyEP9nonbonded",
    "_ZN11ObjectArenaIcE11getNewArrayEi",
    "_ZN20ComputeNonbondedUtil16calc_pair_energyEP9nonbonded",

    /* leela */
    "_ZNSt6vectorIhSaIhEE17_M_default_appendEm",
    "_ZNSt27__uninitialized_default_n_1ILb1EE18__uninit_default_nIPhmEET_S3_T0_",
    "_ZSt10__fill_n_aIPhmhET_S1_T0_RKT1_St26random_access_iterator_tag", // key
    "_ZSt9__fill_a1IhEN9__gnu_cxx11__enable_ifIXsr9__is_byteIT_EE7__valueEvE6__typeEPS2_S5_RKS2_",
    "_ZN9SGFParser14chop_from_fileENSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEi",
    "_ZN9SGFParser5parseERNSt7__cxx1119basic_istringstreamIcSt11char_traitsIcESaIcEEEP7SGFTree",
    "_ZN9SGFParser19parse_property_nameERNSt7__cxx1119basic_istringstreamIcSt11char_traitsIcESaIcEEE",
    "_ZN9SGFParser20parse_property_valueERNSt7__cxx1119basic_istringstreamIcSt11char_traitsIcESaIcEEERNS0_12basic_stringIcS3_S4_EE",
    "_ZNSt8_Rb_treeINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_S5_ESt10_Select1stIS8_ESt4lessIS5_ESaIS8_EE13_Rb_tree_implISC_Lb1EEC2ERKSG_",
    "_ZNKSt8_Rb_treeINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_S5_ESt10_Select1stIS8_ESt4lessIS5_ESaIS8_EE7_M_rootEv",
    "_ZNKSt8_Rb_treeINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_S5_ESt10_Select1stIS8_ESt4lessIS5_ESaIS8_EE9_M_mbeginEv",
    "_ZN9__gnu_cxx17__normal_iteratorIPKiSt6vectorIiSaIiEEEppEv",
    "_ZN5boost5arrayIN9FastBoard8square_tELm441EEixEm",
    "_ZN7UCTNode15create_childrenER9FastStateb",
    "_ZN5boost5arrayItLm441EEixEm",
    "_ZNSt6vectorIiSaIiEE9push_backERKi",
    "_ZNSt6vectorIiSaIiEE17_M_realloc_insertIJRKiEEEvN9__gnu_cxx17__normal_iteratorIPiS1_EEDpOT_",
    "_ZSt11stable_sortIN9__gnu_cxx17__normal_iteratorIPN5boost6tuples5tupleIiiP7UCTNodeNS3_9null_typeES7_S7_S7_S7_S7_S7_EESt6vectorIS8_SaIS8_EEEE8NodeCompEvT_SF_T0_", // no
    "_ZSt13__stable_sortIN9__gnu_cxx17__normal_iteratorIPN5boost6tuples5tupleIiiP7UCTNodeNS3_9null_typeES7_S7_S7_S7_S7_S7_EESt6vectorIS8_SaIS8_EEEENS0_5__ops15_Iter_comp_iterI8NodeCompEEEvT_SI_T0_", // no
    "_ZSt22__stable_sort_adaptiveIN9__gnu_cxx17__normal_iteratorIPN5boost6tuples5tupleIiiP7UCTNodeNS3_9null_typeES7_S7_S7_S7_S7_S7_EESt6vectorIS8_SaIS8_EEEES9_NS0_5__ops15_Iter_comp_iterI8NodeCompEEEvT_SI_SI_T0_T1_", // no
    "_ZSt24__merge_sort_with_bufferIN9__gnu_cxx17__normal_iteratorIPN5boost6tuples5tupleIiiP7UCTNodeNS3_9null_typeES7_S7_S7_S7_S7_S7_EESt6vectorIS8_SaIS8_EEEES9_NS0_5__ops15_Iter_comp_iterI8NodeCompEEEvT_SI_T0_T1_",
    "_ZSt17__merge_sort_loopIN9__gnu_cxx17__normal_iteratorIPN5boost6tuples5tupleIiiP7UCTNodeNS3_9null_typeES7_S7_S7_S7_S7_S7_EESt6vectorIS8_SaIS8_EEEES9_lNS0_5__ops15_Iter_comp_iterI8NodeCompEEEvT_SI_T0_T1_T2_",
    "_ZSt12__move_mergeIN9__gnu_cxx17__normal_iteratorIPN5boost6tuples5tupleIiiP7UCTNodeNS3_9null_typeES7_S7_S7_S7_S7_S7_EESt6vectorIS8_SaIS8_EEEES9_NS0_5__ops15_Iter_comp_iterI8NodeCompEEET0_T_SJ_SJ_SJ_SI_T1_",
    "_ZNSt11__copy_moveILb1ELb0ESt26random_access_iterator_tagE8__copy_mIPN5boost6tuples5tupleIiiP7UCTNodeNS4_9null_typeES8_S8_S8_S8_S8_S8_EESA_EET0_T_SC_SB_",
    "_ZSt17__merge_sort_loopIPN5boost6tuples5tupleIiiP7UCTNodeNS1_9null_typeES5_S5_S5_S5_S5_S5_EEN9__gnu_cxx17__normal_iteratorIS7_St6vectorIS6_SaIS6_EEEElNS8_5__ops15_Iter_comp_iterI8NodeCompEEEvT_SI_T0_T1_T2_",
    "_ZSt12__move_mergeIPN5boost6tuples5tupleIiiP7UCTNodeNS1_9null_typeES5_S5_S5_S5_S5_S5_EEN9__gnu_cxx17__normal_iteratorIS7_St6vectorIS6_SaIS6_EEEENS8_5__ops15_Iter_comp_iterI8NodeCompEEET0_T_SJ_SJ_SJ_SI_T1_",
    "_ZSt30__move_merge_adaptive_backwardIN9__gnu_cxx17__normal_iteratorIPN5boost6tuples5tupleIiiP7UCTNodeNS3_9null_typeES7_S7_S7_S7_S7_S7_EESt6vectorIS8_SaIS8_EEEES9_SD_NS0_5__ops15_Iter_comp_iterI8NodeCompEEEvT_SI_T0_SJ_T1_T2_",
    "_ZNSt20__copy_move_backwardILb1ELb0ESt26random_access_iterator_tagE13__copy_move_bIPN5boost6tuples5tupleIiiP7UCTNodeNS4_9null_typeES8_S8_S8_S8_S8_S8_EESA_EET0_T_SC_SB_",

    /* x264 */
    "init_contexts",
    "lzma_code",
    
    /* perlbench */
    "Perl_more_bodies",
    "Perl_init_constants",
    "Perl_sv_upgrade",
    "S_new_slab",
    "Perl_ck_svconst",
    "Perl_sv_clear",
    "Perl_fbm_compile",
    "S_scan_word",
    "Perl_sv_setsv_flags",
    "Perl_sv_setsv_cow",
    "S_sv_uncow",
    "S_reg_set_capture_string",
    "Perl_re_intuit_start",
    "Perl_regexec_flags",
    "S_scan_const",
    "Perl_magic_set",
    "Perl_scan_version",
    "Perl_scan_version2",
    "S_gen_constant_list",
    "Perl_sv_2pv_flags",
    "S_uiv_2buf",
    "Perl_av_extend_guts",
    "Perl_av_shift",
    "Perl_sv_utf8_upgrade_flags_grow",
    "S_scan_ident",
    "S_sv_display",
    "Perl_pp_split",
    "Perl_sv_inc_nomg",
    "Perl_leave_scope",
    "S_regmatch",
    "csh_glob",
    "Perl_pp_fttext",
    "Perl_rninstr",
    "S_pack_rec",
    "Perl_pp_pushmark",
    "bsd_glob",
    "doencodes",
    "S_maybe_multideref",
    "S_mergesortsv",
    "Perl_scan_vstring",
    "dynprep",
    "S_mergesortsv",
    "glob0",
    "Perl_pp_refgen",
    "Perl_markstack_grow",
    "glob1",
    "glob2",
    "glob3",
    "globextend",
    "Perl_pp_sort",
    "S_unpack_rec",
    "S_reverse_copy",
    "S_regatom",
    "Perl_init_stacks",
    "S_utf8_to_bytes",
    "Perl_utf8_hop",
    "Perl_padnamelist_store",

    /* parest */
    "_ZL14get_input_fileB5cxx11iPPc",
    "_Z20get_output_directoryRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
    "_ZStplIcSt11char_traitsIcESaIcEENSt7__cxx1112basic_stringIT_T0_T1_EEOS8_PKS5_",
    "_ZStplIcSt11char_traitsIcESaIcEENSt7__cxx1112basic_stringIT_T0_T1_EEPKS5_RKS8_",
    "_ZNKSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE13get_allocatorEv",
    "_ZSt12__str_concatINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEET_PKNS6_10value_typeENS6_9size_typeES9_SA_RKNS6_14allocator_typeE",
    "_ZN12METomography20TomographyParametersILi3EE18declare_parametersERN6dealii16ParameterHandlerE",
    "_ZN12METomography8Geometry21default_geometry_nameILi3EEENSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEv",
    "_ZN12METomography7Targets12target_namesILi3EEENSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEv",

    /* xalan */
    "_ZN11xalanc_1_1011XalanVectorItNS_31MemoryManagedConstructionTraitsItEEE6resizeEmRKt",
    "_ZN11xalanc_1_1011XalanVectorItNS_31MemoryManagedConstructionTraitsItEEE10endPointerEv",
    "_ZN11xalanc_1_1011XalanVectorItNS_31MemoryManagedConstructionTraitsItEEE7destroyEPtS4_",
    "_ZN11xalanc_1_1011XalanVectorItNS_31MemoryManagedConstructionTraitsItEEEaSERKS3_",
    "_ZN11xalanc_1_1011XalanVectorItNS_31MemoryManagedConstructionTraitsItEEE6insertEPtPKtS6_",
    "_ZNK11xalanc_1_1011XalanVectorItNS_31MemoryManagedConstructionTraitsItEEE10endPointerEv",
};

static RegisterPass<BaggyBounds::BaggyBoundsPass>
    X("baggy-bounds", "Baggy Bounds initialization pass",
      false /* Only looks at CFG */,
      false /* Analysis Pass */);
