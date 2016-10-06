//===-- UnisonDriver.cpp - Unison driver pass -----------------------------===//
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
// This pass replaces LLVM's register allocation and instruction scheduling
// with the Unison pipeline. It assumes the following executables are on the
// PATH:
//  - uni (Unison)
//  - gecode-presolver
//  - gecode-solver
//
// The pass assumes that it is run right before emission and gets as input
// the name of a MIR file that has been generated with the input to Unison
// (see Passes.cpp).
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_UNISONDRIVER_UNISONDRIVER_H
#define LLVM_CODEGEN_UNISONDRIVER_UNISONDRIVER_H

#define DEBUG_TYPE "unison"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Program.h"
#include "llvm/CodeGen/MIRParser/MIRParser.h"
#include "llvm/Support/SourceMgr.h"
#include <sstream>

using namespace llvm;

namespace llvm {

  class UnisonDriver : public MachineFunctionPass {

    class ProgramPath {
    public:
    ProgramPath() : m_path("") {}
      void load(const char* pgm);
      std::string getPath() const { return m_path; }
    private:
      std::string m_path;
    };

    class Command {
    public:
      Command() = delete;
      explicit Command(ProgramPath &pgm, std::vector<std::string> argv);
      bool run();
      void print();
    private:
      std::string m_cmd;
      std::vector<std::string> m_args;
      StringRef m_sink;
      const StringRef* m_redir[3]; // in, out, err
    };

    std::string PreMir; // File path to Unison input

    ProgramPath UnisonPath;
    ProgramPath PresolverPath;
    ProgramPath SolverPath;

    std::string Target;


    std::string AsmMir; //basefile path
    SmallVector<std::string, 8> TempPaths;

  public:

    static char ID; // Pass identification

    UnisonDriver();
    UnisonDriver(StringRef Pre);
    ~UnisonDriver();

    void getAnalysisUsage(AnalysisUsage &AU) const;

    bool runOnMachineFunction(MachineFunction &MF);

    // Tells whether module M has a function X with a
    // __attribute__((annotate("unison"))) annotation, where X is any
    // function if F == nullptr, or F otherwise.
    static bool hasUnisonAnnotation(const Module * M,
                                    const Function * F = nullptr);

  private:

    void ensure(bool res, const char* msg);

    std::string makeTempFile(const char* Suffix);

    bool runTool(const char* tool, std::string input, std::string output,
                 std::vector<std::string> & extra);

    void cleanPaths();

    void insertFlags(std::vector<std::string> & argv, std::string & flags,
                     bool lintFlag = false);

  };

}

#endif // LLVM_CODEGEN_UNISONDRIVER_UNISONDRIVER_H
