#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
class LoopReportPass : public PassInfoMixin<LoopReportPass> {
 public:
  PreservedAnalyses run(Function& F, FunctionAnalysisManager& AM) {
    if (F.isDeclaration()) return PreservedAnalyses::all();

    const auto& LI = AM.getResult<LoopAnalysis>(F);
    auto& SE = AM.getResult<ScalarEvolutionAnalysis>(F);
    for (const Loop* L : LI.getLoopsInPreorder()) {
      unsigned Instructions = 0;
      unsigned Loads = 0;
      unsigned Stores = 0;
      unsigned Calls = 0;
      for (const BasicBlock* BB : L->blocks()) {
        for (const Instruction& I : *BB) {
          ++Instructions;
          Loads += isa<LoadInst>(I);
          Stores += isa<StoreInst>(I);
          Calls += isa<CallBase>(I);
        }
      }
      const unsigned TripCount = SE.getSmallConstantTripCount(L);
      errs() << "LOOP_REPORT"
             << " function=" << F.getName()
             << " depth=" << L->getLoopDepth()
             << " blocks=" << L->getNumBlocks()
             << " instructions=" << Instructions
             << " loads=" << Loads
             << " stores=" << Stores
             << " calls=" << Calls
             << " constant_trip_count=";
      if (TripCount == 0)
        errs() << "unknown";
      else
        errs() << TripCount;
      errs() << '\n';
    }
    return PreservedAnalyses::all();
  }
};
}  // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "LoopReport", LLVM_VERSION_STRING,
          [](PassBuilder& PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager& FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name != "loop-report") return false;
                  FPM.addPass(LoopReportPass());
                  return true;
                });
          }};
}
