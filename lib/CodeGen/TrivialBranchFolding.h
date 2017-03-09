//===-- TrivialBranchFolding.h -------------------------------  -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_CODEGEN_TRIVIALBRANCHFOLDING_H
#define LLVM_LIB_CODEGEN_TRIVIALBRANCHFOLDING_H

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/Support/BlockFrequency.h"
#include <vector>

namespace llvm {
  class MachineBlockFrequencyInfo;
  class MachineFunction;
  class MachineModuleInfo;
  class TargetInstrInfo;
  class TargetRegisterInfo;

  class LLVM_LIBRARY_VISIBILITY TrivialBranchFolder {
  public:
    explicit TrivialBranchFolder(void);

    bool OptimizeFunction(MachineFunction &MF,
                          const TargetInstrInfo *tii,
                          const TargetRegisterInfo *tri,
                          MachineModuleInfo *mmi);
  private:
    const TargetInstrInfo *TII;
    const TargetRegisterInfo *TRI;
    MachineModuleInfo *MMI;
    DenseMap<const MachineBasicBlock *, int> FuncletMembership;

    bool OptimizeBranches(MachineFunction &MF);
    bool OptimizeBlock(MachineBasicBlock *MBB);
    void RemoveDeadBlock(MachineBasicBlock *MBB);
  };
}

#endif /* LLVM_CODEGEN_TRIVIALBRANCHFOLDING_HPP */
