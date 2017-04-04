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
  static bool isLastInstrCopySP = false;
  static unsigned int vregSpCopy = 0;
  const MachineFunction *MF = MI->getParent()->getParent();
  const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();
  const TargetRegisterInfo *TRI = MF->getSubtarget().getRegisterInfo();

  StringRef instr_name = TII->getName(MI->getOpcode());

  // Check if the instruction copies the stack pointer
  if (instr_name.equals("COPY")) {
    MachineOperand& def_op = MI->getOperand(0);
    MachineOperand& use_op = MI->getOperand(1);
    if (def_op.isReg() && use_op.isReg()) {
      unsigned int reg = use_op.getReg();
      if (reg < TRI->getNumRegs()) {
        StringRef reg_name = TRI->getName(reg);
        if (reg_name.equals("R29")) {
          isLastInstrCopySP = true;
          vregSpCopy = def_op.getReg();
          return 0;
        }
      }
    }
  }

  if ( instr_name.equals("ADJCALLSTACKDOWN") ||
       instr_name.equals("ADJCALLSTACKUP")
     ) {
      return 0;
  }

  // Check if the instruction accesses the stack
  for (unsigned op = 0; op < MI->getNumOperands(); ++op) {
    if (MI->getOperand(op).getType() == MachineOperand::MO_FrameIndex) {
      return 0;
    }
  }

  // Check if the instruction stores the stack pointer
  if (MI->mayStore()) {
      for (unsigned op = 0; op < MI->getNumOperands(); ++op) {
          MachineOperand& mop = MI->getOperand(op);
          if ( isLastInstrCopySP &&
               mop.isReg() &&
               TRI->isVirtualRegister(mop.getReg()) &&
               mop.getReg() == vregSpCopy
             ) {
              return 0;
          }
      }
  }

  isLastInstrCopySP = false;

  return model->computeInstrLatency(MI);
}
