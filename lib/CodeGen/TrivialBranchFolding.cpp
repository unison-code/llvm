//===-- TrivialBranchFolding.cpp - Sub-register extraction pass. ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass removes terminators that redundantly jump to the successor block.
//
// This is a stripped-down version of BranchFolding.cpp.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "trivial-branch-folding"
#include "TrivialBranchFolding.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/CodeGen/Analysis.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineBlockFrequencyInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/Target/TargetSubtargetInfo.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

namespace {
  class TrivialBranchFolding : public MachineFunctionPass {
  public:
    static char ID; // Pass identification

    TrivialBranchFolding() : MachineFunctionPass(ID) {}

    bool runOnMachineFunction(MachineFunction &MF) override;

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      MachineFunctionPass::getAnalysisUsage(AU);
    }
  };
} // end anonymous namespace

char TrivialBranchFolding::ID = 0;
char &llvm::TrivialBranchFoldingID = TrivialBranchFolding::ID;

INITIALIZE_PASS_BEGIN(TrivialBranchFolding, "trivial-branch-folding",
                      "Trivial branch folding", false, false)
INITIALIZE_PASS_END(TrivialBranchFolding, "trivial-branch-folding",
                    "Trivial branch folding", false, false)

bool TrivialBranchFolding::runOnMachineFunction(MachineFunction &MF) {
  TrivialBranchFolder Folder;
  return Folder.OptimizeFunction(MF, MF.getSubtarget().getInstrInfo(),
                                 MF.getSubtarget().getRegisterInfo(),
                                 getAnalysisIfAvailable<MachineModuleInfo>());
}

TrivialBranchFolder::TrivialBranchFolder(void) {}

bool TrivialBranchFolder::OptimizeFunction(MachineFunction &MF,
                                           const TargetInstrInfo *tii,
                                           const TargetRegisterInfo *tri,
                                           MachineModuleInfo *mmi) {
  if (!tii) return false;

  TII = tii;
  TRI = tri;
  MMI = mmi;

  bool MadeChange = false;
  bool MadeChangeThisIteration;
  do {
    MadeChangeThisIteration = OptimizeBranches(MF);
    MadeChange |= MadeChangeThisIteration;
  } while (MadeChangeThisIteration);

  return MadeChange;
}

bool TrivialBranchFolder::OptimizeBranches(MachineFunction &MF) {
  bool MadeChange = false;

  // Make sure blocks are numbered in order
  MF.RenumberBlocks();
  // Renumbering blocks alters funclet membership, recalculate it.
  FuncletMembership = getFuncletMembership(MF);

  for (MachineFunction::iterator I = std::next(MF.begin()), E = MF.end();
       I != E; ) {
    MachineBasicBlock *MBB = &*I++;
    MadeChange |= OptimizeBlock(MBB);

    // If it is dead, remove it.
    if (MBB->pred_empty()) {
      RemoveDeadBlock(MBB);
      MadeChange = true;
    }
  }

  return MadeChange;
}

// Blocks should be considered empty if they contain only debug info;
// else the debug info would affect codegen.
static bool IsEmptyBlock(MachineBasicBlock *MBB) {
  return MBB->getFirstNonDebugInstr() == MBB->end();
}

// Blocks with only debug info and branches should be considered the same
// as blocks with only branches.
static bool IsBranchOnlyBlock(MachineBasicBlock *MBB) {
  MachineBasicBlock::iterator I = MBB->getFirstNonDebugInstr();
  assert(I != MBB->end() && "empty block!");
  return I->isBranch();
}

/// IsBetterFallthrough - Return true if it would be clearly better to
/// fall-through to MBB1 than to fall through into MBB2.  This has to return
/// a strict ordering, returning true for both (MBB1,MBB2) and (MBB2,MBB1) will
/// result in infinite loops.
static bool IsBetterFallthrough(MachineBasicBlock *MBB1,
                                MachineBasicBlock *MBB2) {
  // Right now, we use a simple heuristic.  If MBB2 ends with a call, and
  // MBB1 doesn't, we prefer to fall through into MBB1.  This allows us to
  // optimize branches that branch to either a return block or an assert block
  // into a fallthrough to the return.
  MachineBasicBlock::iterator MBB1I = MBB1->getLastNonDebugInstr();
  MachineBasicBlock::iterator MBB2I = MBB2->getLastNonDebugInstr();
  if (MBB1I == MBB1->end() || MBB2I == MBB2->end())
    return false;

  // If there is a clear successor ordering we make sure that one block
  // will fall through to the next
  if (MBB1->isSuccessor(MBB2)) return true;
  if (MBB2->isSuccessor(MBB1)) return false;

  return MBB2I->isCall() && !MBB1I->isCall();
}

/// getBranchDebugLoc - Find and return, if any, the DebugLoc of the branch
/// instructions on the block.
static DebugLoc getBranchDebugLoc(MachineBasicBlock &MBB) {
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I != MBB.end() && I->isBranch())
    return I->getDebugLoc();
  return DebugLoc();
}

/// OptimizeBlock - Analyze and optimize control flow related to the specified
/// block.  This is never called on the entry block.
bool TrivialBranchFolder::OptimizeBlock(MachineBasicBlock *MBB) {
  bool MadeChange = false;
  MachineFunction &MF = *MBB->getParent();
ReoptimizeBlock:

  MachineFunction::iterator FallThrough = MBB->getIterator();
  ++FallThrough;

  // Make sure MBB and FallThrough belong to the same funclet.
  bool SameFunclet = true;
  if (!FuncletMembership.empty() && FallThrough != MF.end()) {
    auto MBBFunclet = FuncletMembership.find(MBB);
    assert(MBBFunclet != FuncletMembership.end());
    auto FallThroughFunclet = FuncletMembership.find(&*FallThrough);
    assert(FallThroughFunclet != FuncletMembership.end());
    SameFunclet = MBBFunclet->second == FallThroughFunclet->second;
  }

  // If this block is empty, make everyone use its fall-through, not the block
  // explicitly.  Landing pads should not do this since the landing-pad table
  // points to this block.  Blocks with their addresses taken shouldn't be
  // optimized away.
  if (IsEmptyBlock(MBB) && !MBB->isEHPad() && !MBB->hasAddressTaken() &&
      SameFunclet) {
    // Dead block?  Leave for cleanup later.
    if (MBB->pred_empty()) return MadeChange;

    if (FallThrough == MF.end()) {
      // TODO: Simplify preds to not branch here if possible!
    } else if (FallThrough->isEHPad()) {
      // Don't rewrite to a landing pad fallthough.  That could lead to the case
      // where a BB jumps to more than one landing pad.
      // TODO: Is it ever worth rewriting predecessors which don't already
      // jump to a landing pad, and so can safely jump to the fallthrough?
    } else {
      // Rewrite all predecessors of the old block to go to the fallthrough
      // instead.
      while (!MBB->pred_empty()) {
        MachineBasicBlock *Pred = *(MBB->pred_end()-1);
        Pred->ReplaceUsesOfBlockWith(MBB, &*FallThrough);
      }
      // If MBB was the target of a jump table, update jump tables to go to the
      // fallthrough instead.
      if (MachineJumpTableInfo *MJTI = MF.getJumpTableInfo())
        MJTI->ReplaceMBBInJumpTables(MBB, &*FallThrough);
      MadeChange = true;
    }
    return MadeChange;
  }

  // Check to see if we can simplify the terminator of the block before this
  // one.
  MachineBasicBlock &PrevBB = *std::prev(MachineFunction::iterator(MBB));

  MachineBasicBlock *PriorTBB = nullptr, *PriorFBB = nullptr;
  SmallVector<MachineOperand, 4> PriorCond;
  bool PriorUnAnalyzable =
    TII->AnalyzeBranch(PrevBB, PriorTBB, PriorFBB, PriorCond, true);
  if (!PriorUnAnalyzable) {
    // If the CFG for the prior block has extra edges, remove them.
    MadeChange |= PrevBB.CorrectExtraCFGEdges(PriorTBB, PriorFBB,
                                              !PriorCond.empty());

    // If the previous branch is conditional and both conditions go to the same
    // destination, remove the branch, replacing it with an unconditional one or
    // a fall-through.
    if (PriorTBB && PriorTBB == PriorFBB) {
      DebugLoc dl = getBranchDebugLoc(PrevBB);
      TII->RemoveBranch(PrevBB);
      PriorCond.clear();
      if (PriorTBB != MBB)
        TII->InsertBranch(PrevBB, PriorTBB, nullptr, PriorCond, dl);
      MadeChange = true;
      goto ReoptimizeBlock;
    }

    // If the previous block unconditionally falls through to this block and
    // this block has no other predecessors, move the contents of this block
    // into the prior block. This doesn't usually happen when SimplifyCFG
    // has been used, but it can happen if tail merging splits a fall-through
    // predecessor of a block.
    // This has to check PrevBB->succ_size() because EH edges are ignored by
    // AnalyzeBranch.
    if (PriorCond.empty() && !PriorTBB && MBB->pred_size() == 1 &&
        PrevBB.succ_size() == 1 &&
        !MBB->hasAddressTaken() && !MBB->isEHPad()) {
      DEBUG(dbgs() << "\nMerging into block: " << PrevBB
                   << "From MBB: " << *MBB);
      // Remove redundant DBG_VALUEs first.
      if (PrevBB.begin() != PrevBB.end()) {
        MachineBasicBlock::iterator PrevBBIter = PrevBB.end();
        --PrevBBIter;
        MachineBasicBlock::iterator MBBIter = MBB->begin();
        // Check if DBG_VALUE at the end of PrevBB is identical to the
        // DBG_VALUE at the beginning of MBB.
        while (PrevBBIter != PrevBB.begin() && MBBIter != MBB->end()
               && PrevBBIter->isDebugValue() && MBBIter->isDebugValue()) {
          if (!MBBIter->isIdenticalTo(PrevBBIter))
            break;
          MachineInstr *DuplicateDbg = MBBIter;
          ++MBBIter; -- PrevBBIter;
          DuplicateDbg->eraseFromParent();
        }
      }
      PrevBB.splice(PrevBB.end(), MBB, MBB->begin(), MBB->end());
      PrevBB.removeSuccessor(PrevBB.succ_begin());
      assert(PrevBB.succ_empty());
      PrevBB.transferSuccessors(MBB);
      MadeChange = true;
      return MadeChange;
    }

    // If the previous branch *only* branches to *this* block (conditional or
    // not) remove the branch.
    if (PriorTBB == MBB && !PriorFBB) {
      TII->RemoveBranch(PrevBB);
      MadeChange = true;
      goto ReoptimizeBlock;
    }

    // If the prior block branches somewhere else on the condition and here if
    // the condition is false, remove the uncond second branch.
    if (PriorFBB == MBB) {
      DebugLoc dl = getBranchDebugLoc(PrevBB);
      TII->RemoveBranch(PrevBB);
      TII->InsertBranch(PrevBB, PriorTBB, nullptr, PriorCond, dl);
      MadeChange = true;
      goto ReoptimizeBlock;
    }

    // If the prior block branches here on true and somewhere else on false, and
    // if the branch condition is reversible, reverse the branch to create a
    // fall-through.
    if (PriorTBB == MBB) {
      SmallVector<MachineOperand, 4> NewPriorCond(PriorCond);
      if (!TII->ReverseBranchCondition(NewPriorCond)) {
        DebugLoc dl = getBranchDebugLoc(PrevBB);
        TII->RemoveBranch(PrevBB);
        TII->InsertBranch(PrevBB, PriorFBB, nullptr, NewPriorCond, dl);
        MadeChange = true;
        goto ReoptimizeBlock;
      }
    }

    // If this block has no successors (e.g. it is a return block or ends with
    // a call to a no-return function like abort or __cxa_throw) and if the pred
    // falls through into this block, and if it would otherwise fall through
    // into the block after this, move this block to the end of the function.
    //
    // We consider it more likely that execution will stay in the function (e.g.
    // due to loops) than it is to exit it.  This asserts in loops etc, moving
    // the assert condition out of the loop body.
    if (MBB->succ_empty() && !PriorCond.empty() && !PriorFBB &&
        MachineFunction::iterator(PriorTBB) == FallThrough &&
        !MBB->canFallThrough()) {
      bool DoTransform = true;

      // We have to be careful that the succs of PredBB aren't both no-successor
      // blocks.  If neither have successors and if PredBB is the second from
      // last block in the function, we'd just keep swapping the two blocks for
      // last.  Only do the swap if one is clearly better to fall through than
      // the other.
      if (FallThrough == --MF.end() &&
          !IsBetterFallthrough(PriorTBB, MBB))
        DoTransform = false;

      if (DoTransform) {
        // Reverse the branch so we will fall through on the previous true cond.
        SmallVector<MachineOperand, 4> NewPriorCond(PriorCond);
        if (!TII->ReverseBranchCondition(NewPriorCond)) {
          DEBUG(dbgs() << "\nMoving MBB: " << *MBB
                       << "To make fallthrough to: " << *PriorTBB << "\n");

          DebugLoc dl = getBranchDebugLoc(PrevBB);
          TII->RemoveBranch(PrevBB);
          TII->InsertBranch(PrevBB, MBB, nullptr, NewPriorCond, dl);

          // Move this block to the end of the function.
          MBB->moveAfter(&MF.back());
          MadeChange = true;
          return MadeChange;
        }
      }
    }
  }

  // Analyze the branch in the current block.
  MachineBasicBlock *CurTBB = nullptr, *CurFBB = nullptr;
  SmallVector<MachineOperand, 4> CurCond;
  bool CurUnAnalyzable= TII->AnalyzeBranch(*MBB, CurTBB, CurFBB, CurCond, true);
  if (!CurUnAnalyzable) {
    // If the CFG for the prior block has extra edges, remove them.
    MadeChange |= MBB->CorrectExtraCFGEdges(CurTBB, CurFBB, !CurCond.empty());

    // If this is a two-way branch, and the FBB branches to this block, reverse
    // the condition so the single-basic-block loop is faster.  Instead of:
    //    Loop: xxx; jcc Out; jmp Loop
    // we want:
    //    Loop: xxx; jncc Loop; jmp Out
    if (CurTBB && CurFBB && CurFBB == MBB && CurTBB != MBB) {
      SmallVector<MachineOperand, 4> NewCond(CurCond);
      if (!TII->ReverseBranchCondition(NewCond)) {
        DebugLoc dl = getBranchDebugLoc(*MBB);
        TII->RemoveBranch(*MBB);
        TII->InsertBranch(*MBB, CurFBB, CurTBB, NewCond, dl);
        MadeChange = true;
        goto ReoptimizeBlock;
      }
    }

    // If this branch is the only thing in its block, see if we can forward
    // other blocks across it.
    if (CurTBB && CurCond.empty() && !CurFBB &&
        IsBranchOnlyBlock(MBB) && CurTBB != MBB &&
        !MBB->hasAddressTaken() && !MBB->isEHPad()) {
      DebugLoc dl = getBranchDebugLoc(*MBB);
      // This block may contain just an unconditional branch.  Because there can
      // be 'non-branch terminators' in the block, try removing the branch and
      // then seeing if the block is empty.
      TII->RemoveBranch(*MBB);
      // If the only things remaining in the block are debug info, remove these
      // as well, so this will behave the same as an empty block in non-debug
      // mode.
      if (IsEmptyBlock(MBB)) {
        // Make the block empty, losing the debug info (we could probably
        // improve this in some cases.)
        MBB->erase(MBB->begin(), MBB->end());
      }
      // If this block is just an unconditional branch to CurTBB, we can
      // usually completely eliminate the block.  The only case we cannot
      // completely eliminate the block is when the block before this one
      // falls through into MBB and we can't understand the prior block's branch
      // condition.
      if (MBB->empty()) {
        bool PredHasNoFallThrough = !PrevBB.canFallThrough();
        if (PredHasNoFallThrough || !PriorUnAnalyzable ||
            !PrevBB.isSuccessor(MBB)) {
          // If the prior block falls through into us, turn it into an
          // explicit branch to us to make updates simpler.
          if (!PredHasNoFallThrough && PrevBB.isSuccessor(MBB) &&
              PriorTBB != MBB && PriorFBB != MBB) {
            if (!PriorTBB) {
              assert(PriorCond.empty() && !PriorFBB &&
                     "Bad branch analysis");
              PriorTBB = MBB;
            } else {
              assert(!PriorFBB && "Machine CFG out of date!");
              PriorFBB = MBB;
            }
            DebugLoc pdl = getBranchDebugLoc(PrevBB);
            TII->RemoveBranch(PrevBB);
            TII->InsertBranch(PrevBB, PriorTBB, PriorFBB, PriorCond, pdl);
          }

          // Iterate through all the predecessors, revectoring each in-turn.
          size_t PI = 0;
          bool DidChange = false;
          bool HasBranchToSelf = false;
          while(PI != MBB->pred_size()) {
            MachineBasicBlock *PMBB = *(MBB->pred_begin() + PI);
            if (PMBB == MBB) {
              // If this block has an uncond branch to itself, leave it.
              ++PI;
              HasBranchToSelf = true;
            } else {
              DidChange = true;
              PMBB->ReplaceUsesOfBlockWith(MBB, CurTBB);
              // If this change resulted in PMBB ending in a conditional
              // branch where both conditions go to the same destination,
              // change this to an unconditional branch (and fix the CFG).
              MachineBasicBlock *NewCurTBB = nullptr, *NewCurFBB = nullptr;
              SmallVector<MachineOperand, 4> NewCurCond;
              bool NewCurUnAnalyzable = TII->AnalyzeBranch(*PMBB, NewCurTBB,
                      NewCurFBB, NewCurCond, true);
              if (!NewCurUnAnalyzable && NewCurTBB && NewCurTBB == NewCurFBB) {
                DebugLoc pdl = getBranchDebugLoc(*PMBB);
                TII->RemoveBranch(*PMBB);
                NewCurCond.clear();
                TII->InsertBranch(*PMBB, NewCurTBB, nullptr, NewCurCond, pdl);
                MadeChange = true;
                PMBB->CorrectExtraCFGEdges(NewCurTBB, nullptr, false);
              }
            }
          }

          // Change any jumptables to go to the new MBB.
          if (MachineJumpTableInfo *MJTI = MF.getJumpTableInfo())
            MJTI->ReplaceMBBInJumpTables(MBB, CurTBB);
          if (DidChange) {
            MadeChange = true;
            if (!HasBranchToSelf) return MadeChange;
          }
        }
      }

      // Add the branch back if the block is more than just an uncond branch.
      TII->InsertBranch(*MBB, CurTBB, nullptr, CurCond, dl);
    }
  }

  // If the prior block doesn't fall through into this block, and if this
  // block doesn't fall through into some other block, see if we can find a
  // place to move this block where a fall-through will happen.
  if (!PrevBB.canFallThrough()) {

    // Now we know that there was no fall-through into this block, check to
    // see if it has a fall-through into its successor.
    bool CurFallsThru = MBB->canFallThrough();

    if (!MBB->isEHPad()) {
      // Check all the predecessors of this block.  If one of them has no fall
      // throughs, move this block right after it.
      for (MachineBasicBlock *PredBB : MBB->predecessors()) {
        // Analyze the branch at the end of the pred.
        MachineBasicBlock *PredTBB = nullptr, *PredFBB = nullptr;
        SmallVector<MachineOperand, 4> PredCond;
        if (PredBB != MBB && !PredBB->canFallThrough() &&
            !TII->AnalyzeBranch(*PredBB, PredTBB, PredFBB, PredCond, true)
            && (!CurFallsThru || !CurTBB || !CurFBB)
            && (!CurFallsThru || MBB->getNumber() >= PredBB->getNumber())) {
          // If the current block doesn't fall through, just move it.
          // If the current block can fall through and does not end with a
          // conditional branch, we need to append an unconditional jump to
          // the (current) next block.  To avoid a possible compile-time
          // infinite loop, move blocks only backward in this case.
          // Also, if there are already 2 branches here, we cannot add a third;
          // this means we have the case
          // Bcc next
          // B elsewhere
          // next:
          if (CurFallsThru) {
            MachineBasicBlock *NextBB = &*std::next(MBB->getIterator());
            CurCond.clear();
            TII->InsertBranch(*MBB, NextBB, nullptr, CurCond, DebugLoc());
          }
          MBB->moveAfter(PredBB);
          MadeChange = true;
          goto ReoptimizeBlock;
        }
      }
    }

    if (!CurFallsThru) {
      // Check all successors to see if we can move this block before it.
      for (MachineBasicBlock *SuccBB : MBB->successors()) {
        // Analyze the branch at the end of the block before the succ.
        MachineFunction::iterator SuccPrev = --SuccBB->getIterator();

        // If this block doesn't already fall-through to that successor, and if
        // the succ doesn't already have a block that can fall through into it,
        // and if the successor isn't an EH destination, we can arrange for the
        // fallthrough to happen.
        if (SuccBB != MBB && &*SuccPrev != MBB &&
            !SuccPrev->canFallThrough() && !CurUnAnalyzable &&
            !SuccBB->isEHPad()) {
          MBB->moveBefore(SuccBB);
          MadeChange = true;
          goto ReoptimizeBlock;
        }
      }

      // Okay, there is no really great place to put this block.  If, however,
      // the block before this one would be a fall-through if this block were
      // removed, move this block to the end of the function.
      MachineBasicBlock *PrevTBB = nullptr, *PrevFBB = nullptr;
      SmallVector<MachineOperand, 4> PrevCond;
      // We're looking for cases where PrevBB could possibly fall through to
      // FallThrough, but if FallThrough is an EH pad that wouldn't be useful
      // so here we skip over any EH pads so we might have a chance to find
      // a branch target from PrevBB.
      while (FallThrough != MF.end() && FallThrough->isEHPad())
        ++FallThrough;
      // Now check to see if the current block is sitting between PrevBB and
      // a block to which it could fall through.
      if (FallThrough != MF.end() &&
          !TII->AnalyzeBranch(PrevBB, PrevTBB, PrevFBB, PrevCond, true) &&
          PrevBB.isSuccessor(&*FallThrough)) {
        MBB->moveAfter(&MF.back());
        MadeChange = true;
        return MadeChange;
      }
    }
  }

  return MadeChange;
}

/// RemoveDeadBlock - Remove the specified dead machine basic block from the
/// function, updating the CFG.
void TrivialBranchFolder::RemoveDeadBlock(MachineBasicBlock *MBB) {
  assert(MBB->pred_empty() && "MBB must be dead!");
  DEBUG(dbgs() << "\nRemoving MBB: " << *MBB);

  MachineFunction *MF = MBB->getParent();
  // drop all successors.
  while (!MBB->succ_empty())
    MBB->removeSuccessor(MBB->succ_end()-1);

  // Remove the block.
  MF->erase(MBB);
  FuncletMembership.erase(MBB);
}
