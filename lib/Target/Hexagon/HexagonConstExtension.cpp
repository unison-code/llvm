//===--- HexagonConstExtension.cpp ----------------------------------------===//
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
// This pass marks constant-extended instructions with metadata for Unison.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "hexagon-ce"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "HexagonTargetMachine.h"

using namespace llvm;

namespace llvm {
  FunctionPass *createHexagonConstExtension();
  void initializeHexagonConstExtensionPass(PassRegistry& Registry);
}

namespace {

  class HexagonConstExtension : public MachineFunctionPass {
  public:
    static char ID;
    HexagonConstExtension() : MachineFunctionPass(ID) {
      initializeHexagonConstExtensionPass(*PassRegistry::getPassRegistry());
    }
    const char *getPassName() const override {
      return "Hexagon constant extension";
    }
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
      MachineFunctionPass::getAnalysisUsage(AU);
    }
    bool runOnMachineFunction(MachineFunction &MF) override;
  };

  char HexagonConstExtension::ID = 0;
}

INITIALIZE_PASS(HexagonConstExtension, "hexagon-ce",
  "Hexagon constant extension", false, false)

bool HexagonConstExtension::runOnMachineFunction(MachineFunction &MF) {
  const HexagonInstrInfo *HII = MF.getSubtarget<HexagonSubtarget>().getInstrInfo();
  bool Changed = false;
  for (auto & MBB : MF) {
    for (auto & MI : MBB) {
      if (!MI.isTransient() &&
          (HII->isExtended(&MI) || HII->isConstExtended(&MI))) {
        LLVMContext &Context = MF.getFunction()->getContext();
        MDBuilder builder(Context);
        MDNode * MD =
          MDNode::get(Context,
                      {builder.createString("unison-property"),
                       builder.createString("constant-extended")});
        MI.addOperand(MF, MachineOperand::CreateMetadata(MD));
        Changed = true;
      }
    }
  }
  return Changed;
}

//===----------------------------------------------------------------------===//
//                         Public Constructor Functions
//===----------------------------------------------------------------------===//
FunctionPass *llvm::createHexagonConstExtension() {
  return new HexagonConstExtension();
}

