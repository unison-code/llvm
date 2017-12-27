//===-------- MachineCodeStats.cpp - Collects statistics about the code----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass collects and reports different counts of machine code.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/MachineCodeStats.h"

using namespace llvm;

#define DEBUG_TYPE "machine-stats"

STATISTIC(BasicBlocksPreRA,   "Number of basic blocks (pre-RA)");
STATISTIC(MachineInstsPreRA,  "Number of machine instructions (pre-RA)");
STATISTIC(VirtualRegsPreRA,   "Number of virtual registers (pre-RA)");
STATISTIC(MaxBlockInstsPreRA, "Maximum number of instructions in a block (pre-RA)");
STATISTIC(CopyInstsPreRA,     "Number of copy instructions (pre-RA)");
STATISTIC(CallInstsPreRA,     "Number of call instructions (pre-RA)");
STATISTIC(RegClassesPreRA,    "Number of register classes (pre-RA)");

STATISTIC(BasicBlocksPostRA,   "Number of basic blocks (post-RA)");
STATISTIC(MachineInstsPostRA,  "Number of machine instructions (post-RA)");
STATISTIC(VirtualRegsPostRA,   "Number of virtual registers (post-RA)");
STATISTIC(MaxBlockInstsPostRA, "Maximum number of instructions in a block (post-RA)");
STATISTIC(CallInstsPostRA,     "Number of call instructions (post-RA)");
STATISTIC(CopyInstsPostRA,     "Number of copy instructions (post-RA)");

char MachineCodeStats::ID = 0;

INITIALIZE_PASS_BEGIN(MachineCodeStats, "machine-stats",
                "Machine code statistics", false, false)
INITIALIZE_PASS_END(MachineCodeStats, "machine-stats",
                "Machine code statistics", false, false)

bool MachineCodeStats::runOnMachineFunction(MachineFunction &MF) {

  const MachineRegisterInfo & MRI = MF.getRegInfo();
  const TargetRegisterInfo * TRI = MF.getSubtarget().getRegisterInfo();

  unsigned maxBlockInstsPre = 0;

  std::set<unsigned> VirtualRegs;

  for (MachineFunction::iterator MBB = MF.begin(), MBBE = MF.end();
       MBB != MBBE; ++MBB) {
    unsigned int blockInsts = 0;
    switch (p) {
    case PreRA:
      ++BasicBlocksPreRA; break;
    case PostRA:
      ++BasicBlocksPostRA; break;
    }
    for (MachineBasicBlock::iterator MI = MBB->begin(), MIE = MBB->end();
         MI != MIE; ++MI) {
      switch (p) {
      case PreRA:
        ++MachineInstsPreRA; break;
      case PostRA:
        ++MachineInstsPostRA; break;
      }
      if (MI->isCopy()) {
        switch (p) {
        case PreRA:
          ++CopyInstsPreRA; break;
        case PostRA:
          ++CopyInstsPostRA; break;
        }
      }
      if (MI->isCall()) {
        switch (p) {
        case PreRA:
          ++CallInstsPreRA; break;
        case PostRA:
          ++CallInstsPostRA; break;
        }
      }
      ++blockInsts;
      for (unsigned int op = 0; op < MI->getNumOperands(); op++) {
        MachineOperand MO = MI->getOperand(op);
        if (MO.isReg() && MO.isDef()) {
          switch (p) {
          case PreRA:
            ++VirtualRegsPreRA; break;
          case PostRA:
            ++VirtualRegsPostRA; break;
          }
        }
        if (MO.isReg()) {
          unsigned Reg = MO.getReg();
          if (TargetRegisterInfo::isVirtualRegister(Reg))
            VirtualRegs.insert(Reg);
        }
      }
    }
    if (blockInsts > maxBlockInstsPre) maxBlockInstsPre = blockInsts;
  }

  switch (p) {
  case PreRA:
    MaxBlockInstsPreRA = maxBlockInstsPre; break;
  case PostRA:
    MaxBlockInstsPostRA = maxBlockInstsPre; break;
  }

  if (p == PreRA && !VirtualRegs.empty()) {
    std::set<unsigned> regClasses;
    std::vector<unsigned> weights;
    for (unsigned Reg : VirtualRegs) {
      regClasses.insert(MRI.getRegClass(Reg)->getID());
      weights.push_back(TRI->getRegClassWeight(MRI.getRegClass(Reg)).RegWeight);
    }
    RegClassesPreRA = regClasses.size();
    double weightMean = mean(weights);
    double weightStdDev = stddev(weights, weightMean);
    double weightCV = weightStdDev / weightMean;
    // We would like to use the STATISTIC mechanism, but it is limited to
    // unsigned integers. This does the trick by now...
    std::stringstream wcv;
    wcv << std::fixed << weightCV;
    errs() << wcv.str() << " "
           << "machine-stats - CV of the register class weights (pre-RA)\n";
  }

  return false;
}

namespace llvm {

  MachineFunctionPass *createMachineCodeStatsPass(
                         MachineCodeStats::StatsPoint p) {
    return new MachineCodeStats(p);
  }

} // end namespace llvm
