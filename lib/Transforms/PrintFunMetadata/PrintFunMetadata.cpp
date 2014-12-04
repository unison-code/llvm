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

#define DEBUG_TYPE "printfunmetadata"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

namespace {

struct PrintFunMetadata : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    PrintFunMetadata() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function& f) {
        errs() << "{\n"
               << "  fname: \"" << f.getName() << "\",\n"
               << "  blocks: [\n";

        BFI = &getAnalysis<BlockFrequencyInfo>();
        for (Function::iterator i = f.begin(); i != f.end(); ++i) {
            errs() << "    ";
            runOnBasicBlock(*i);
            if (i != --f.end()) errs() << ",";
            errs() << "\n";
        }

        errs() << "  ]\n"
               << "}\n";

        return false;
    }

    // We don't modify the program, so we preserve all analyses.
    virtual void getAnalysisUsage(AnalysisUsage& AU) const {
        AU.setPreservesAll();
        AU.addRequired<BlockFrequencyInfo>();
    }

  protected:
    /// Invoked on every basic block inside the function.
    void runOnBasicBlock(BasicBlock& bb) {
        errs() << "[ \"" << bb.getName()
               << "\", " << BFI->getBlockFreq(&bb)
               << " ]";
    }

  private:
    BlockFrequencyInfo* BFI;

};

}

char PrintFunMetadata::ID = 0;
static RegisterPass<PrintFunMetadata>
CP("print-fun-metadata", "PrintFunMetadata Pass");
