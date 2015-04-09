//===-- llvm/CodeGen/ISelCost.cpp -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass computes and prints the cost of instruction selection according to
// Unison's cost model.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "expand-isel-pseudos"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Target/TargetMachine.h"
using namespace llvm;

namespace {
  class ISelCost : public MachineFunctionPass {
  public:
    static char ID; // Pass identification, replacement for typeid
    ISelCost() : MachineFunctionPass(ID) {}

  private:
    virtual bool runOnMachineFunction(MachineFunction &MF);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      MachineFunctionPass::getAnalysisUsage(AU);
    }
  };
} // end anonymous namespace

char ISelCost::ID = 0;
char &llvm::ISelCostID = ISelCost::ID;
INITIALIZE_PASS(ISelCost, "isel-cost",
                "Compute and print the cost of instruction selection",
                false, false)

bool ISelCost::runOnMachineFunction(MachineFunction &MF) {

  int size = 0;

  // Iterate through each instruction in the function
  for (MachineFunction::iterator I = MF.begin(), E = MF.end(); I != E; ++I) {
    MachineBasicBlock *MBB = I;
    for (MachineBasicBlock::iterator MBBI = MBB->begin(), MBBE = MBB->end();
         MBBI != MBBE; ) {
      MachineInstr *MI = MBBI++;

      size += MI->getDesc().getSize();

    }
  }

  errs() << "{\"size\": " << size << "}\n";

  return false;

}
