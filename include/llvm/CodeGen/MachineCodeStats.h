//===-------- MachineCodeStats.h - Collects statistics about the code----=====//
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

#ifndef LLVM_CODEGEN_MCODESTATS_H
#define LLVM_CODEGEN_MCODESTATS_H

#include <set>
#include <vector>
#include <sstream>
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
  class MachineCodeStats : public MachineFunctionPass {

  public:

    enum StatsPoint { PreRA, PostRA };

    static char ID; // Pass identification
    MachineCodeStats() : MachineFunctionPass(ID), p(PreRA) {
      initializeMachineCodeStatsPass(*PassRegistry::getPassRegistry());
    }
    MachineCodeStats(StatsPoint p0) : MachineFunctionPass(ID), p(p0) {
      initializeMachineCodeStatsPass(*PassRegistry::getPassRegistry());
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesCFG();
      AU.addPreservedID(MachineDominatorsID);
      MachineFunctionPass::getAnalysisUsage(AU);
    }

    virtual bool runOnMachineFunction(MachineFunction &MF);

  private:

    StatsPoint p;

    template<typename T>
    static T mean(const std::vector<T> & v) {
      T m = 0.0;
      for (auto & e : v) m += (T)e;
      return m / (T)v.size();
    }

    template<typename T>
    static T stddev(const std::vector<unsigned> & v, T m) {
      std::vector<T> diffs;
      for (auto & e : v) {
        T diff = (T)e - m;
        diffs.push_back(diff * diff);
      }
      T variance = mean(diffs);
      return sqrt(variance);
    }

  };

  MachineFunctionPass *createMachineCodeStatsPass(
                         MachineCodeStats::StatsPoint p);

} // end namespace llvm

#endif
