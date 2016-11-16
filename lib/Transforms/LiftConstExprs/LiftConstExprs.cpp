//===- LiftConstExprs.cpp - Lifts constant expressions into instructions -  --//
//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
// Extended by Gabriel Hjort Blindell <ghb@kth.se>
//
//===----------------------------------------------------------------------===//
//
// This pass lifts all constant expressions into instructions. This is needed
// for uni-is as it expects all instructions to only consist of simple
// expressions (that is, a constant value or a temporary).
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"

#include "llvm/IR/Dominators.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "../../IR/ConstantsContext.h"

#include <iostream>
#include <map>
#include <utility>

using namespace llvm;

namespace {

struct LiftConstExprs : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid

    LiftConstExprs() : FunctionPass(ID) {}

    const char*
    getPassName() const {
        return "Lifts all constant expressions into instructions";
    }

    virtual bool
    runOnFunction (Function& F);

    virtual void
    getAnalysisUsage(AnalysisUsage& AU) const {
      // This pass does not modify the control-flow graph of the function
      AU.setPreservesCFG();
    }
};

/**
 * Checks if a given value has a constant expression that can be lifted.
 *
 * @param V
 *        Value to check.
 *
 * @returns NULL if the value has no such expression, otherwise a pointer to the
 * value casted into a ConstantExpr.
 */
static ConstantExpr*
hasLiftableConstExpr(Value* V) {
    if (ConstantExpr* CE = dyn_cast<ConstantExpr>(V)) {
        switch (CE->getOpcode()) {
            case Instruction::ICmp:
            case Instruction::FCmp:
            case Instruction::PtrToInt:
            case Instruction::IntToPtr:
            case Instruction::Trunc:
            case Instruction::ZExt:
            case Instruction::SExt:
            case Instruction::FPTrunc:
            case Instruction::FPExt:
            case Instruction::UIToFP:
            case Instruction::SIToFP:
            case Instruction::FPToUI:
            case Instruction::FPToSI:
            case Instruction::AddrSpaceCast:
            case Instruction::BitCast:
            case Instruction::Select:
            case Instruction::ExtractElement:
            case Instruction::ExtractValue:
            case Instruction::InsertElement:
            case Instruction::InsertValue:
            case Instruction::ShuffleVector:
            case Instruction::GetElementPtr: {
                return CE;
            }

            default: {
                // Check if any of its operands has a liftable expression
                for (unsigned index = 0;
                     index < CE->getNumOperands();
                     ++index)
                {
                    if (hasLiftableConstExpr(CE->getOperand(index))) return CE;
                }
                break;
            }
        }
    }

    return NULL;
}

/**
 * Converts a constant expression into a corresponding instruction. This
 * function does *not* perform any recursion, so the resulting instruction may
 * still have constant expression operands.
 *
 * @param CE
 *        Constant expression to lift.
 * @param InsertPt
 *        The instruction before which to insert the new instruction.
 *
 * @returns A pointer to the new instruction.
 */
static Instruction*
liftConstExpr(ConstantExpr* CE, Instruction* InsertPt) {
    switch (CE->getOpcode()) {
        // Unary operations
        case Instruction::AddrSpaceCast:
        case Instruction::BitCast:
        case Instruction::FPExt:
        case Instruction::FPToSI:
        case Instruction::FPToUI:
        case Instruction::FPTrunc:
        case Instruction::IntToPtr:
        case Instruction::PtrToInt:
        case Instruction::SExt:
        case Instruction::SIToFP:
        case Instruction::Trunc:
        case Instruction::UIToFP:
        case Instruction::ZExt: {
            return CastInst::Create(Instruction::CastOps(CE->getOpcode()),
                                    CE->getOperand(0),
                                    CE->getType(),
                                    CE->getName(),
                                    InsertPt);
        }

        // Binary operations
        case Instruction::Add:
        case Instruction::Sub:
        case Instruction::Mul:
        case Instruction::UDiv:
        case Instruction::SDiv:
        case Instruction::FDiv:
        case Instruction::URem:
        case Instruction::SRem:
        case Instruction::FRem:
        case Instruction::Shl:
        case Instruction::AShr:
        case Instruction::And:
        case Instruction::Or:
        case Instruction::Xor: {
            Instruction::BinaryOps op = Instruction::BinaryOps(CE->getOpcode());
            return BinaryOperator::Create(op,
                                          CE->getOperand(0),
                                          CE->getOperand(1),
                                          CE->getName(),
                                          InsertPt);
        }

        // Compare operations
        case Instruction::FCmp:
        case Instruction::ICmp: {
            CompareConstantExpr* CMP = dyn_cast<CompareConstantExpr>(CE);
            return CmpInst::Create(Instruction::OtherOps(CE->getOpcode()),
                                   CmpInst::Predicate(CMP->predicate),
                                   CE->getOperand(0),
                                   CE->getOperand(1),
                                   CE->getName(),
                                   InsertPt);
        }

        case Instruction::ExtractElement: {
            return ExtractElementInst::Create(CE->getOperand(0),
                                              CE->getOperand(1),
                                              CE->getName(),
                                              InsertPt);
        }

        case Instruction::GetElementPtr: {
            std::vector<Value*> Indices_vec;
            for (unsigned index = 1; index < CE->getNumOperands(); ++index) {
                Indices_vec.push_back (CE->getOperand (index));
            }
            ArrayRef<Value*> Indices(Indices_vec);

            return GetElementPtrInst::Create(NULL,
                                             CE->getOperand(0),
                                             Indices,
                                             CE->getName(),
                                             InsertPt);
        }

        case Instruction::InsertElement: {
            return InsertElementInst::Create(CE->getOperand(0),
                                             CE->getOperand(1),
                                             CE->getOperand(2),
                                             CE->getName(),
                                             InsertPt);
        }

        case Instruction::InsertValue: {
            InsertValueConstantExpr* IV = dyn_cast<InsertValueConstantExpr>(CE);
            return InsertValueInst::Create(CE->getOperand(0),
                                           CE->getOperand(1),
                                           IV->Indices,
                                           CE->getName(),
                                           InsertPt);
        }

        case Instruction::Select: {
            return SelectInst::Create(CE->getOperand(0),
                                      CE->getOperand(1),
                                      CE->getOperand(2),
                                      CE->getName(),
                                      InsertPt);
        }

        default: {
            assert(0 && "Unhandled constant expression!\n");
            break;
        }
    }
}

bool
LiftConstExprs::runOnFunction(Function& F) {
    bool modified = false;

    // Worklist of values to check for constant expressions
    std::vector<Instruction*> Worklist;

    // Initialize the worklist by finding all instructions that have one or more
    // operands containing a constant expression
    for (Function::iterator BB = F.begin(); BB != F.end(); ++BB) {
        for (BasicBlock::iterator i = BB->begin(); i != BB->end(); ++i) {
            Instruction* I = &(*i);
            for (unsigned index = 0; index < I->getNumOperands(); ++index) {
                if (hasLiftableConstExpr(I->getOperand(index))) {
                    Worklist.push_back (I);
                }
            }
        }
    }

    // Determine whether we will modify anything.
    if (Worklist.size() > 0) modified = true;

    // While the worklist is not empty, take an item from it, convert the
    // operands into instructions if necessary, and determine if the newly
    // added instructions need to be processed as well
    while (Worklist.size()) {
        Instruction* I = Worklist.back();
        Worklist.pop_back();

        // Scan through the operands of this instruction and convert each into
        // an instruction. Note that this works a little differently for phi
        // instructions because the new instruction must be added to the
        // appropriate predecessor block
        if (PHINode* PHI = dyn_cast<PHINode>(I)) {
            for (unsigned index = 0;
                 index < PHI->getNumIncomingValues();
                 ++index)
            {
                // For PHI Nodes, if an operand is a constant expression, we
                // want to insert the new instructions in the predecessor basic
                // block.
                //
                // Note: It seems that it's possible for a phi to have the same
                // incoming basic block listed multiple times; this seems okay
                // as long the same value is listed for the incoming block.
                Instruction* InsertPt =
                    PHI->getIncomingBlock(index)->getTerminator();
                if (ConstantExpr* CE =
                    hasLiftableConstExpr(PHI->getIncomingValue(index)))
                {
                    Instruction* NewInst = liftConstExpr(CE, InsertPt);
                    for (unsigned i2 = index;
                         i2 < PHI->getNumIncomingValues();
                         ++i2)
                    {
                        if ((PHI->getIncomingBlock (i2)) ==
                            PHI->getIncomingBlock (index))
                        {
                            PHI->setIncomingValue (i2, NewInst);
                        }
                    }
                    Worklist.push_back (NewInst);
                }
            }
        }
        else {
            for (unsigned index = 0; index < I->getNumOperands(); ++index) {
                // For other instructions, we want to insert instructions
                // replacing constant expressions immediently before the
                // instruction using the constant expression
                if (ConstantExpr* CE =
                    hasLiftableConstExpr(I->getOperand(index)))
                {
                    Instruction* NewInst = liftConstExpr(CE, I);
                    I->replaceUsesOfWith (CE, NewInst);
                    Worklist.push_back (NewInst);
                }
            }
        }
    }

    return modified;
}

}

char LiftConstExprs::ID = 0;

static RegisterPass<LiftConstExprs>
P("lift-const-exprs", "Lifts all constant expressions into instructions");
