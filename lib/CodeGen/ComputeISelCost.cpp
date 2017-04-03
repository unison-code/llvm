//===-- llvm/CodeGen/DumpISelWCosts.cpp -------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Functions for computing cost of instructions during instruction selection.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/TargetSchedule.h"
#include "llvm/Target/TargetInstrInfo.h"

using namespace llvm;

int getInstrCost(TargetSchedModel* model, MachineInstr* MI) {
  const MachineFunction *MF = MI->getParent()->getParent();
  const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();
  StringRef name = TII->getName(MI->getOpcode());

  if (name.equals("ADJCALLSTACKDOWN") || name.equals("ADJCALLSTACKUP")) {
      return 0;
  }

  // Check if the instruction accesses the stack
  for (unsigned op = 0; op < MI->getNumOperands(); ++op) {
    if (MI->getOperand(op).getType() == MachineOperand::MO_FrameIndex) {
      return 0;
    }
  }

  return model->computeInstrLatency(MI);
}
