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

#ifndef LLVM_LIB_CODEGEN_COMPUTEISELCOST_H
#define LLVM_LIB_CODEGEN_COMPUTEISELCOST_H

#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/TargetSchedule.h"
#include "llvm/Target/TargetInstrInfo.h"

using namespace llvm;

bool isOperandUsedByPhi(MachineOperand* op);
int getInstrCost(TargetSchedModel* model, MachineInstr* MI);

#endif
