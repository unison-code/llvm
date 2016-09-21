//===- LowerSelect.cpp - Transform select insts to branches ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass lowers select instructions into conditional branches for targets
// that do not have conditional moves or that have not implemented the select
// instruction yet.
//
// Note that this pass could be improved.  In particular it turns every select
// instruction into a new conditional branch, even though some common cases have
// select instructions on the same predicate next to each other.  It would be
// better to use the same branch for the whole group of selects.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "lowerselect"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Compiler.h"
using namespace llvm;

namespace {
  /// LowerSelect - Turn select instructions into conditional branches.
  ///
  struct LowerSelect : public FunctionPass {
  public:
    static char ID; // Pass identification, replacement for typeid
    LowerSelect() : FunctionPass(ID) {}

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      // This certainly destroys the CFG.
      // This is a cluster of orthogonal Transforms:
      AU.addPreserved<UnifyFunctionExitNodes>();
      AU.addPreservedID(LowerSwitchID);
      AU.addPreservedID(LowerInvokePassID);
    }

    bool runOnFunction(Function &F) {
      bool Changed = false;
      for (Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
        for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
          if (SelectInst *SI = dyn_cast<SelectInst>(I)) {
            if (SI->getCondition()->getType()->isIntegerTy(1)) {
              // Lower only scalar select constructs

              // Get execution frequency metadata of this block
              MDNode *MD = BB->getTerminator()->getMetadata("exec_freq");

              // Split this basic block in half right before the select
              // instruction.
              BasicBlock *NewCont =
                BB->splitBasicBlock(I, BB->getName()+".selectcont");

              // Make the true block, and make it branch to the continue block.
              BasicBlock *NewTrue =
                BasicBlock::Create(SI->getContext(),
                                   BB->getName()+".selecttrue",
                                   BB->getParent(), NewCont);

              BranchInst* new_br = BranchInst::Create(NewCont, NewTrue);
              if (MD) new_br->setMetadata("exec_freq", MD);

              // Make the unconditional branch in the incoming block be a
              // conditional branch on the select predicate.
              BB->getInstList().erase(BB->getTerminator());

              BranchInst* new_condbr = BranchInst::Create(NewTrue,
                                                          NewCont,
                                                          SI->getCondition(),
                                                          &(*BB));
              if (MD) new_condbr->setMetadata("exec_freq", MD);

              // Create a new PHI node in the cont block with the entries we
              // need.
              PHINode *PN =
                PHINode::Create(SI->getType(), 0, "", &(*NewCont->begin()));
              PN->takeName(SI);
              PN->addIncoming(SI->getTrueValue(), NewTrue);
              PN->addIncoming(SI->getFalseValue(), &(*BB));

              // Use the PHI instead of the select.
              SI->replaceAllUsesWith(PN);
              NewCont->getInstList().erase(SI);

              Changed = true;
              break; // This block is done with.
            }
          }
        }
      return Changed;
    }

  };

}

//===----------------------------------------------------------------------===//
// This pass converts SelectInst instructions into conditional branch and PHI
// instructions.
//



char LowerSelect::ID = 0;
static RegisterPass<LowerSelect>
X("lowerselect", "Lower select instructions to branches");
