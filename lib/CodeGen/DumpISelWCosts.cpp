//===-- llvm/CodeGen/DumpISelWCosts.cpp -------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass computes and prints the cost of instruction selection according to
// Unison's cost model.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "dump-isel-w-costs"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/TargetSchedule.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;

namespace {

  class DumpISelWCosts : public MachineFunctionPass {
  public:
    static char ID; // Pass identification, replacement for typeid
    DumpISelWCosts() : MachineFunctionPass(ID) {}

  private:

    TargetSchedModel SchedModel;

    virtual bool runOnMachineFunction(MachineFunction &MF);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      MachineFunctionPass::getAnalysisUsage(AU);
    }
  };
} // end anonymous namespace

char DumpISelWCosts::ID = 0;
char &llvm::DumpISelWCostsID = DumpISelWCosts::ID;
INITIALIZE_PASS(DumpISelWCosts, "dump-isel-w-costs",
                "Dumps result of instruction selection, with costs attached",
                false, false)

FormattedNumber toCostString(int c) {
  return format_decimal(c, 3);
}

bool DumpISelWCosts::runOnMachineFunction(MachineFunction &MF) {

  const TargetSubtargetInfo &ST = MF.getSubtarget();
  TargetSchedModel model;
  model.init(ST.getSchedModel(), &ST, ST.getInstrInfo());

  // Iterate through each instruction in the function
  for (MachineFunction::iterator I = MF.begin(), E = MF.end(); I != E; ++I) {
    MachineBasicBlock *MBB = &(*I);
    assert(MBB);
    const MDNode *fn =
        MBB->getBasicBlock()->getTerminator()->getMetadata("exec_freq");
    assert(fn);
    const ConstantAsMetadata *c_md_f =
      (const ConstantAsMetadata *)(fn->getOperand(0).get());
    assert(c_md_f);
    const ConstantInt *ci_f = (const ConstantInt *)c_md_f->getValue();
    int f = ci_f->getLimitedValue();

    errs() << toCostString(f) << ": " << MBB->getFullName() << "\n";

    for (MachineBasicBlock::iterator MBBI = MBB->begin(), MBBE = MBB->end();
         MBBI != MBBE; ) {
      MachineInstr *MI = MBBI++;
      int c =  model.computeInstrLatency(MI);
      errs() << toCostString(c) << ":    ";
      MI->print(errs());
    }
  }

  return false;

}
