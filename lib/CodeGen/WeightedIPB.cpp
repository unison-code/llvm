//===-- WeightedIPB.cpp - weighted instructions per bundle ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This analysis computes instructions per bundle weighted by estimated
// execution frequency.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "weightedipb"
#include <map>
#include <sstream>
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"
#include "llvm/ADT/Statistic.h"

#include "SpillPlacement.h"

using namespace llvm;

namespace {

  class WeightedIPB : public MachineFunctionPass {
  public:
    static char ID; // Pass identification
    WeightedIPB() : MachineFunctionPass(ID) {
      initializeWeightedIPBPass(*PassRegistry::getPassRegistry());
    }
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesAll();
      AU.addRequired<SpillPlacement>();
      MachineFunctionPass::getAnalysisUsage(AU);
    }

  private:
    virtual bool runOnMachineFunction(MachineFunction&);
  };

} // end anonymous namespace

char WeightedIPB::ID = 0;
char &llvm::WeightedIPBID = WeightedIPB::ID;

INITIALIZE_PASS_BEGIN(WeightedIPB, "weighted-ipb",
                      "Weigthed IPB", false, false)
INITIALIZE_PASS_END(WeightedIPB, "weighted-ipb",
                    "Weigthed IPB", false, false)

bool WeightedIPB::runOnMachineFunction(MachineFunction &MF) {

  const SpillPlacement * SP = getAnalysisIfAvailable<SpillPlacement>();

  std::map<int,double> ipb;

  double totalFreq = 0.0, weightedFreq = 0.0;

  for (MachineFunction::iterator MBB = MF.begin(), MBBE = MF.end();
       MBB != MBBE; ++MBB) {
    long int blockInstructions = 0, blockBundles = 0;
    bool realBundles = false; // MIPS marks NOP instructions as "insideBundle"
    for (auto MI = MBB->instr_begin(), MIE = MBB->instr_end();
         MI != MIE; ++MI) {
      if (MI->isBundle()) {
        realBundles = true;
        blockBundles++;
      } else if (realBundles && MI->isInsideBundle()) {
        blockInstructions++;
      } else { // See this as a singleton bundle
        blockInstructions++;
        blockBundles++;
      }
    }
    ipb[MBB->getNumber()] = (double)blockInstructions / (double)blockBundles;
    totalFreq += (double)SP->getBlockFrequency(MBB->getNumber()).getFrequency();
    SP->getBlockFrequency(MBB->getNumber()).getFrequency();
  }

  for (MachineFunction::iterator MBB = MF.begin(), MBBE = MF.end();
       MBB != MBBE; ++MBB) {
    if (!MBB->empty()) {
      double scale =
        (double)SP->getBlockFrequency(MBB->getNumber()).getFrequency() / totalFreq;
      weightedFreq += ipb[MBB->getNumber()] * scale;
    }
  }

  // We would like to use the STATISTIC mechanism, but it is limited to unsigned
  // integers. This does the trick by now...
  std::stringstream wf;
  wf << std::fixed << weightedFreq;
  errs() << wf.str() << " "
         << "weightedipb - Weighted instructions per bundle\n";

  // We never change the function.
  return false;

}
