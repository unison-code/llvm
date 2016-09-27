//===-- CleanUnisonMetadata.cpp - Unison metadata cleanup -----------------===//
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
// This pass cleans all machine operand metadata created by Unison.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "clean-unison-metadata"
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

  class CleanUnisonMetadata : public MachineFunctionPass {
  public:
    static char ID; // Pass identification
    CleanUnisonMetadata() : MachineFunctionPass(ID) {
      initializeCleanUnisonMetadataPass(*PassRegistry::getPassRegistry());
    }
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesAll();
      MachineFunctionPass::getAnalysisUsage(AU);
    }
  private:
    virtual bool runOnMachineFunction(MachineFunction&);
  };

} // end anonymous namespace

char CleanUnisonMetadata::ID = 0;
char &llvm::CleanUnisonMetadataID = CleanUnisonMetadata::ID;

INITIALIZE_PASS_BEGIN(CleanUnisonMetadata, "clean-unison-metadata",
                      "Clean Unison Metadata", false, false)
INITIALIZE_PASS_END(CleanUnisonMetadata, "clean-unison-metadata",
                    "Clean Unison Metadata", false, false)

bool CleanUnisonMetadata::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  for (auto & MBB : MF) {
    for (auto & MI : MBB) {
      while (true) {
        bool Removed = false;
        for (unsigned i = 0, n = MI.getNumOperands(); i != n; ++i) {
          auto MO = MI.getOperand(i);
          if (MO.isMetadata()) {
            const auto MD = MO.getMetadata();
            if (MD->getNumOperands() >= 1) {
              auto K = dyn_cast<MDString>(MD->getOperand(0).get());
              if (K && K->getString().startswith("unison")) {
                MI.RemoveOperand(i);
                Changed = true;
                Removed = true;
                break;
              }
            }
          }
        }
        if (Removed == true) continue;
        else break;
      }
    }
  }
  return Changed;
}
