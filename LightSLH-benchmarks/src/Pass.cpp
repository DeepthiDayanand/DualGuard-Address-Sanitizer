#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "LightHardening.h" 
#include "LightFence.h"
#include "LightCut.h"
#include "LightSpectreV3.h"
#include "SSLH.h"
#include "FilterHardening.h"

using namespace llvm;



extern "C" LLVM_ATTRIBUTE_WEAK ::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "LightHardeningPass", "v0.1",
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef PassName, ModulePassManager &MPM, ...) {
                    if (PassName == "lightslh") {
                        MPM.addPass(LightHardeningPass());
                        return true;
                    } else if (PassName == "sslh") { 
                        MPM.addPass(SSLHPass());
                        return true;
                    } else if (PassName == "lightfence") { 
                        MPM.addPass(LightFencePass());
                        return true;
                    } else if (PassName == "lightcut") { 
                        MPM.addPass(LightCutPass());
                        return true;
                    } else if (PassName == "lightv3") { 
                        MPM.addPass(LightV3Pass());
                        return true;
                    } else if (PassName == "filterslh") {
                        MPM.addPass(FilterHardeningPass());
                        return true;
                    }
                    return false;
                }
            );
        }
    };
}
