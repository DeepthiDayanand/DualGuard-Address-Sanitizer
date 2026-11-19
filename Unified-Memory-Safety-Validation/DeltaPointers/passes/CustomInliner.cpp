/*
 * CustomInliner.cpp
 *
 * LLVM 14 port: The legacy Inliner base class and InlinerPass.h were removed.
 * We reimplement as a ModulePass that walks all call sites in the module and
 * calls InlineFunction() on those that are NOINSTRUMENT && AlwaysInline, or
 * whose name contains NOINSTRUMENT_PREFIX "_inline_".
 *
 *  Original authors: haller, taddeus
 */

#include <llvm/IR/Instructions.h>
#include <llvm/Transforms/Utils/Cloning.h>   // InlineFunction, InlineFunctionInfo
#include <llvm/Analysis/InlineCost.h>

#include "Common.h"

using namespace llvm;

struct CustomInliner : public ModulePass {
    static char ID;
    CustomInliner() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
        bool Changed = false;

        // Collect call/invoke sites to inline.  We iterate to a fixed point
        // because inlining a callee can expose additional inlinable callsites
        // inside it.
        bool AnyInlined;
        do {
            AnyInlined = false;
            SmallVector<CallBase *, 16> ToInline;

            for (Function &F : M) {
                for (BasicBlock &BB : F) {
                    for (Instruction &I : BB) {
                        CallBase *CB = dyn_cast<CallBase>(&I);
                        if (!CB)
                            continue;

                        Function *Callee = CB->getCalledFunction();
                        if (!Callee || Callee->isDeclaration())
                            continue;

                        bool ShouldInline = false;

                        if (isNoInstrument(Callee) &&
                            Callee->hasFnAttribute(Attribute::AlwaysInline))
                            ShouldInline = true;

                        if (Callee->hasName() &&
                            Callee->getName().startswith(
                                NOINSTRUMENT_PREFIX "_inline_"))
                            ShouldInline = true;

                        if (ShouldInline)
                            ToInline.push_back(CB);
                    }
                }
            }

            for (CallBase *CB : ToInline) {
                InlineFunctionInfo IFI;
                InlineResult IR = InlineFunction(*CB, IFI);
                if (IR.isSuccess()) {
                    AnyInlined = true;
                    Changed = true;
                }
            }
        } while (AnyInlined);

        return Changed;
    }
};

char CustomInliner::ID = 0;
static RegisterPass<CustomInliner> X("custominline", "Custom Inliner Pass",
                                     true, false);
