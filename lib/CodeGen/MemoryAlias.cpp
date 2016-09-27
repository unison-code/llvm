//===-- MemoryAlias.cpp - Memory Alias Analysis ---------------------------===//
//
//  Main authors:
//    Roberto Castaneda Lozano <rcas@sics.se>
//
//  This file is part of Unison, see http://unison-code.github.io
//
//  Copyright (c) 2016, SICS Swedish ICT AB
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
// This analysis computes which load and store instructions access disjoint
// partitions of memory.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "memoryalias"
#include <map>
#include <cstddef>
#include "llvm/ADT/EquivalenceClasses.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/ADT/EquivalenceClasses.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/CodeGen/ScheduleDAGInstrs.h"

using namespace llvm;

namespace {

  typedef EquivalenceClasses<MachineInstr *> MemAccessPartition;

  class MemoryAlias : public MachineFunctionPass {
    AliasAnalysis *AA;
    std::map<MachineInstr *, unsigned> MP;
  public:
    static char ID; // Pass identification
    MemoryAlias() : MachineFunctionPass(ID) {
      initializeMemoryAliasPass(*PassRegistry::getPassRegistry());
    }
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesAll();
      AU.addRequired<AAResultsWrapperPass>();
      MachineFunctionPass::getAnalysisUsage(AU);
    }
    /// getAccessPartition - Return the access partition of a given instruction
    virtual unsigned getAccessPartition(MachineInstr * MI) const {
      return MP.at(MI);
    }
  private:
    virtual bool runOnMachineFunction(MachineFunction&);
  };

} // end anonymous namespace

char MemoryAlias::ID = 0;
char &llvm::MemoryAliasID = MemoryAlias::ID;

INITIALIZE_PASS_BEGIN(MemoryAlias, "memory-alias",
                      "Memory Alias Analysis", false, false)
INITIALIZE_PASS_END(MemoryAlias, "memory-alias",
                    "Memory Alias Analysis", false, false)

bool MemoryAlias::runOnMachineFunction(MachineFunction &MF) {

  AA = &getAnalysis<AAResultsWrapperPass>().getAAResults();
  const DataLayout &DL = MF.getFunction()->getParent()->getDataLayout();

  bool Changed = false;

  for (MachineFunction::iterator MBB = MF.begin(), MBBE = MF.end();
       MBB != MBBE; ++MBB) {

    // Create initial partitions with all the memory references in the block

    MemAccessPartition MP;
    for (MachineBasicBlock::iterator MI = MBB->begin();
         MI != MBB->end(); ++MI)
      if (!MI->isBundle() && (MI->mayStore() || MI->mayLoad())) MP.insert(MI);

    // Pairwise compare all memory references and merge those which may alias

    for (MemAccessPartition::iterator MI1 = MP.begin(); MI1 != MP.end();
         ++MI1) {
      for (MemAccessPartition::iterator MI2 = MP.begin(); MI2 != MP.end();
           ++MI2) {
        // If MI1 and MI2 may alias
        if ((MI1->getData()->mayStore() || MI2->getData()->mayStore()) &&
            MIsNeedChainEdge(AA, MF.getFrameInfo(), DL,
                             MI1->getData(), MI2->getData())) {
          MP.unionSets(MI1->getData(), MI2->getData());
        }
      }
    }

    unsigned int p = 0;
    for (MemAccessPartition::iterator MA = MP.begin(); MA != MP.end(); ++MA) {
      if (!MA->isLeader()) continue;
      for (MemAccessPartition::member_iterator MI = MP.member_begin(MA);
           MI != MP.member_end(); ++MI) {
        this->MP[*MI] = p;
      }
      p++;
    }

    // Add a debug operand to each memory access instruction with the partition
    // of the memory reference
    for (MachineBasicBlock::iterator MI = MBB->begin();
         MI != MBB->end(); ++MI)
      if (!MI->isBundle() && (MI->mayStore() || MI->mayLoad())) {
        MachineInstr *MemI = MI;
        LLVMContext &Context = MF.getFunction()->getContext();
        MDBuilder builder(Context);
        MDNode * MD =
          MDNode::get(Context,
          {builder.createString("unison-memory-partition"),
           builder.createConstant(
             ConstantInt::get(Type::getInt32Ty(Context), this->MP.at(MemI)))});
        MI->addOperand(MF, MachineOperand::CreateMetadata(MD));
        Changed = true;
      }

  }

  return Changed;

}
