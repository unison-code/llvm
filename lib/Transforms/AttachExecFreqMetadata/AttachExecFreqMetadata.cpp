/**
 * Copyright (c) 2014, Gabriel Hjort Blindell <ghb@kth.se>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define DEBUG_TYPE "attachfunmetadata"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

namespace {

struct AttachExecFreqMetadata : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    AttachExecFreqMetadata() : FunctionPass(ID) {}

    virtual bool
    runOnFunction(Function& f) {
        BFI = &getAnalysis<BlockFrequencyInfoWrapperPass>().getBFI();
        for (Function::iterator i = f.begin(), e = f.end(); i != e; ++i) {
            runOnBasicBlock(*i);
        }

        return false;
    }

    // We don't modify the program, so we preserve all analyses.
    virtual void
    getAnalysisUsage(AnalysisUsage& AU) const {
        AU.setPreservesAll();
        AU.addRequired<BlockFrequencyInfoWrapperPass>();
    }

  protected:
    /// Invoked on every basic block inside the function.
    void
    runOnBasicBlock(BasicBlock& bb) {
        bb.getTerminator()->setMetadata("exec_freq", getBlockFreqMetadata(bb));
    }

    /// Produces basic block frequency information as metadata.
    MDNode*
    getBlockFreqMetadata(BasicBlock& bb) {
        Value* freq_value =
            ConstantInt::get(IntegerType::get(bb.getContext(), 64),
                             getBlockFreq(bb),
                             false);
        Metadata* freq_value_as_meta = ValueAsMetadata::get(freq_value);
        return MDNode::get(bb.getContext(),
                           ArrayRef<Metadata*>(freq_value_as_meta));
    }

    /// Gets the block frequency as an integer.
    uint64_t
    getBlockFreq(BasicBlock& bb) {
      return BFI->getBlockFreq(&bb).getFrequency();
    }

  private:
    BlockFrequencyInfo* BFI;

};

}

char AttachExecFreqMetadata::ID = 0;
static RegisterPass<AttachExecFreqMetadata>
Y("attach-exec-freq-metadata", "AttachExecFreqMetadata Pass");
