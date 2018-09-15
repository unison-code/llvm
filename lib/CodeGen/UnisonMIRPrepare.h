//===-- UnisonMIRPrepare.cpp - Unison-style MIR printing preparation --=======//
//
//  Main authors:
//    Roberto Castaneda Lozano <roberto.castaneda@ri.se>
//
//  This file is part of Unison, see http://unison-code.github.io
//
//  Copyright (c) 2017, RISE SICS AB
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of the copyright holder nor the names of its
//     contributors may be used to endorse or promote products derived from this
//     software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.
//
//===----------------------------------------------------------------------===//
//
/// \file
/// This pass prepares the Machine IR with additional information that needs to
/// be passed to Unison through the MIR representation:
///
///   - the estimated execution frequency of each basic block
///
///   - which load and store instructions access disjoint partitions of memory
///
/// This information is added by extending the IR with additional
/// pseudo-instructions and metadata operands which are interpreted by the MIR
/// printer class (see MIRPrinter.cpp). This pass is assumed to be the last one
/// to be run before printing the MIR and terminating. The memory partition
/// information is only computed for unbundled code.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_UNISONMIRPREPARE_H
#define LLVM_CODEGEN_UNISONMIRPREPARE_H

#include "llvm/ADT/EquivalenceClasses.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include <map>

namespace llvm {

class MachineBlockFrequencyInfo;

typedef EquivalenceClasses<MachineInstr *> MemAccessPartition;

class UnisonMIRPrepare : public MachineFunctionPass {
  const TargetInstrInfo *TII;
  MachineBlockFrequencyInfo *MBFI;
  AliasAnalysis *AA;
  // Memory partition map (from machine instructions to memory partition ids).
  std::map<MachineInstr *, unsigned> MP;

public:
  static char ID;
  UnisonMIRPrepare();
  virtual void getAnalysisUsage(AnalysisUsage &) const override;
  virtual bool runOnMachineFunction(MachineFunction &) override;
  // Add an ANNOTATION_LABEL instruction to the beginning of each basic block
  // annotated with the block's frequency as a metadata operand.
  void annotateFrequency(MachineBasicBlock &);
  // Add a metadata operand to each unbundled memory instruction with the
  // abstract memory partition that the instruction refers to.
  void annotateMemoryPartitions(MachineBasicBlock &);
};

} // end namespace llvm

#endif // LLVM_CODEGEN_UNISONMIRPREPARE_H
