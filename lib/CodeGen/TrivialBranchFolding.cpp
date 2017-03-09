//===-- TrivialBranchFolding.cpp - Sub-register extraction pass. --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass removes terminators that just branch to the fall through block.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "trivial-branch-folding"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Target/TargetSubtargetInfo.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

namespace {
  class TrivialBranchFolding : public MachineFunctionPass {

    const TargetInstrInfo *TII;

  public:
    static char ID; // Pass identification

    TrivialBranchFolding() : MachineFunctionPass(ID) {
      initializeTrivialBranchFoldingPass(*PassRegistry::getPassRegistry());
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      MachineFunctionPass::getAnalysisUsage(AU);
    }

    virtual bool runOnMachineFunction(MachineFunction &MF);
    virtual bool runOnMachineBasicBlock(MachineBasicBlock &MBB);


  private:
  };
} // end anonymous namespace

char TrivialBranchFolding::ID = 0;
char &llvm::TrivialBranchFoldingID = TrivialBranchFolding::ID;

INITIALIZE_PASS_BEGIN(TrivialBranchFolding, "trivial-branch-folding",
                      "Trivial branch folding", false, false)
INITIALIZE_PASS_END(TrivialBranchFolding, "trivial-branch-folding",
                    "Trivial branch folding", false, false)

bool TrivialBranchFolding::runOnMachineFunction(MachineFunction& MF) {

  DEBUG({
      dbgs() << "********** Trivial branch folding **********\n"
             << "********** Function: "
             << MF.getFunction()->getName() << '\n';
    });

  TII = MF.getSubtarget().getInstrInfo();

  bool Changed = false;

  for (MachineFunction::iterator MBB = MF.begin(); MBB != MF.end(); ++MBB)
    Changed |= runOnMachineBasicBlock(*MBB);

  return Changed;
}


bool TrivialBranchFolding::runOnMachineBasicBlock(MachineBasicBlock& MBB) {
  bool Changed = false;

  errs() << MBB;

  if (MBB.canFallThrough()) {
    errs() << "  CHECKING...\n";
    MachineBasicBlock::reverse_iterator MI = MBB.rbegin();
   if (MI != MBB.rend() && MI->isUnconditionalBranch()) {
      MI->eraseFromParent();
      Changed = true;
      errs() << "  * ERASED " << *MI;
    }
  }

  return Changed;
}
