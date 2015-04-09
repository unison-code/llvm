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

#define DEBUG_TYPE "isel-cost"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/TargetSchedule.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Metadata.h"
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

    TargetSchedModel SchedModel;
    const TargetInstrInfo *TII;

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

  TII = MF.getTarget().getInstrInfo();

  const TargetSubtargetInfo &ST =
    MF.getTarget().getSubtarget<TargetSubtargetInfo>();
  SchedModel.init(*ST.getSchedModel(), &ST, TII);

  int cycles = 0;
  int size = 0;

  // Iterate through each instruction in the function
  for (MachineFunction::iterator I = MF.begin(), E = MF.end(); I != E; ++I) {
    MachineBasicBlock *MBB = I;
    // TODO: check for null pointers and fail gracefully
    const MDNode * fn =
      MBB->getBasicBlock()->getTerminator()->getMetadata("exec_freq");
    int f = ((const ConstantInt *)fn->getOperand(0))->getLimitedValue();
    for (MachineBasicBlock::iterator MBBI = MBB->begin(), MBBE = MBB->end();
         MBBI != MBBE; ) {
      MachineInstr *MI = MBBI++;
      cycles +=  f * SchedModel.computeInstrLatency(MI);
      size += MI->getDesc().getSize();
    }
  }

  errs() << "{\n \"cycles\": " << cycles << ",\n \"size\": " << size << "\n}\n";

  return false;

}
