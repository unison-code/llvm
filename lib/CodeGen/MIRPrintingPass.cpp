//===- MIRPrintingPass.cpp - Pass that prints out using the MIR format ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements a pass that prints out the LLVM module using the MIR
// serialization format.
//
//===----------------------------------------------------------------------===//

#include "MIRPrinter.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MIRYamlMapping.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/CodeGen/MachineFunction.h"

#include "SpillPlacement.h"

using namespace llvm;

namespace {

/// This pass prints out the LLVM IR to an output stream using the MIR
/// serialization format.
struct MIRPrintingPass : public MachineFunctionPass {
  static char ID;
  raw_ostream &OS;
  bool UnisonStyle;
  bool FinalizeFunctions;
  std::string MachineFunctions;

  MIRPrintingPass() : MachineFunctionPass(ID), OS(dbgs()),
                      UnisonStyle(false), FinalizeFunctions(false) {
    initializeSpillPlacementPass(*PassRegistry::getPassRegistry());
  }
  MIRPrintingPass(raw_ostream &OS0,
                  bool UnisonStyle0, bool FinalizeFunctions0) :
    MachineFunctionPass(ID), OS(OS0),
    UnisonStyle(UnisonStyle0), FinalizeFunctions(FinalizeFunctions0) {
    initializeSpillPlacementPass(*PassRegistry::getPassRegistry());
  }

  const char *getPassName() const override { return "MIR Printing Pass"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    if (UnisonStyle)
      AU.addRequired<SpillPlacement>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    MIRAuxiliaryInfo info;
    info.UnisonStyle = UnisonStyle;
    info.SP = getAnalysisIfAvailable<SpillPlacement>();
    std::string Str;
    raw_string_ostream StrOS(Str);
    printMIR(StrOS, MF, info);
    MachineFunctions.append(StrOS.str());
    if (FinalizeFunctions) {
      Module * M = const_cast<Module*>(MF.getFunction()->getParent());
      printMIR(OS, *M);
      OS << StrOS.str();
    }
    return false;
  }

  bool doFinalization(Module &M) override {
    printMIR(OS, M);
    OS << MachineFunctions;
    return false;
  }
};

char MIRPrintingPass::ID = 0;

} // end anonymous namespace

char &llvm::MIRPrintingPassID = MIRPrintingPass::ID;
INITIALIZE_PASS(MIRPrintingPass, "mir-printer", "MIR Printer", false, false)

namespace llvm {

  MachineFunctionPass *createPrintMIRPass(raw_ostream &OS,
                                          bool UnisonStyle,
                                          bool FinalizeFunctions) {
    return new MIRPrintingPass(OS, UnisonStyle, FinalizeFunctions);
}

} // end namespace llvm
