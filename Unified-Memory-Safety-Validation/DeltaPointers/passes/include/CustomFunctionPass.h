#ifndef CUSTOM_FUNCTION_PASS_H
#define CUSTOM_FUNCTION_PASS_H

#include "Common.h"

using namespace llvm;

static bool shouldInstrument(Function &F) {
    if (F.isDeclaration())
        return false;

    if (isNoInstrument(&F))
        return false;

    return true;
}

static std::set<std::string> whitelistedFunctions = {
    /* x264 */
    "biari_init_context",
    ".initialize_globals",
    "predict_16x16_dc",
    "predict_16x16_dc_left",
    "predict_16x16_dc_top",
    "predict_16x16_dc_128",
    "predict_16x16_h",
    "predict_16x16_v",
    "predict_16x16_p",
    "predict_8x8c_dc_128",
    "predict_8x8c_dc_left",
    "predict_8x8c_dc_top",
    "predict_8x8c_dc",
    "predict_8x8c_h",
    "predict_8x8c_v",
    "predict_8x8c_p",
    "predict_4x4_dc_128",
    "predict_4x4_dc_left",
    "predict_4x4_dc_top",
    "predict_4x4_dc",
    "predict_4x4_h",
    "predict_4x4_v",
    "predict_4x4_ddl",
    "predict_4x4_ddr",
    "predict_4x4_vr",
    "predict_4x4_hd",
    "predict_4x4_vl",
    "predict_4x4_hu",
    "predict_8x8_filter",
    "predict_8x8_dc_128",
    "predict_8x8_dc_left",
    "predict_8x8_dc_top",
    "predict_8x8_dc",
    "predict_8x8_h",
    "predict_8x8_v",
    "predict_8x8_ddl",
    "predict_8x8_ddr",
    "predict_8x8_vr",
    "predict_8x8_hd",
    "predict_8x8_vl",
    "predict_8x8_hu",
    "x264_predict_16x16_init",
    "x264_predict_8x8c_init",
    "x264_predict_8x8_init",
    "x264_predict_4x4_init",
    "hpel_filter",
    "deblock_luma_c",
    "deblock_chroma_c",
    "deblock_luma_intra_c",
    "deblock_chroma_intra_c",
    "mc_chroma",
    "get_filename_extension",

    /* omnetpp*/
    "_ZN6cEnvir6printfEPKcz",
    "_ZN6cEnvir13flushLastLineEv",
    "_ZNSt15_Rb_tree_header8_M_resetEv",

    /* povray */
    "_ZN8pov_base14ProcessOptions15Parse_CL_OptionERPKcP9POVMSDatab",
    "_ZN8pov_base14ProcessOptions15Parse_CL_StringERPKci",
    "_ZN7MyClass3fooEv",

    /* xalancbmk */
    "_ZN11xalanc_1_1020XalanStdOutputStream9writeDataEPKcj",
    "_ZN11xalanc_1_1020XalanStdOutputStream7doFlushEv",
};

struct CustomFunctionPass : public ModulePass {
    CustomFunctionPass(char &ID) : ModulePass(ID) {}



    bool runOnModule(Module &M) override {
        bool Changed = initializeModule(M);
        Changed |= whitelistFunctions(M);

        for (Function &F : M) {
            if (shouldInstrument(F))
                Changed |= runOnFunction(F);
        }

        Changed |= finalizeModule(M);

        return Changed;
    }

    bool whitelistFunctions(Module &M) {
        bool Changed = false;
        for (const auto &name : whitelistedFunctions) {
            Function *func = M.getFunction(name);
            if (func) {
                setNoInstrument(func);
            }
        }
        return Changed;
    }

protected:
    virtual bool initializeModule(Module &M) { return false; }
    virtual bool runOnFunction(Function &F) = 0;
    virtual bool finalizeModule(Module &M) { return false; }
};

#endif /* !CUSTOM_FUNCTION_PASS_H */
