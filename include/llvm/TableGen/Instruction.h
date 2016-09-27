//===- llvm/TableGen/Instruction.h - Unison Instruction ---------*- C++ -*-===//
//
//  Main authors:
//    Jan Tomljanovic <jan.tomljanovic@sics.se>
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
/// \file
/// This file defines the instruction for Unison in the Instruction class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TABLEGEN_INSTRUCTION_H
#define LLVM_TABLEGEN_INSTRUCTION_H
#include<vector>
#include<string>
#include "llvm/Support/raw_ostream.h"
using namespace std;

// operand can be a register, label or a bound
// if it is a register, the usedef_field and regtype_field are used
namespace unison {
  struct operand {
    string name;
    bool label;
    bool bound;
    string usedef_field;
    string regtype_field;
  };
}

/// Definition of the instruction which contains methods to print it in the
/// .yaml format
class Instruction {

  string id; // name
  string type; // linear | call | branch
  vector<unison::operand> operands;
  vector<string> uses;
  vector<string> defs;
  int size;
  bool affects_mem;
  bool affected_mem;
  vector<string> affects_reg;
  vector<string> affected_reg;
  string itinerary;

public:

  /// Constructor which creates the instruction, all the arguments are necessary
  Instruction(string id, string type, vector<unison::operand> operands,
              vector<string> uses, vector<string> defs,
              int size, bool affects_mem, bool affected_mem,
              vector<string> affects_reg,
              vector<string> affected_reg, string itinerary);

  /// Prints the id in the .yaml format
  void print_id(llvm::raw_ostream &OS);

  /// Prints the type in the .yaml format
  void print_type(llvm::raw_ostream &OS);

  /// Prints operands in the .yaml format
  void print_operands(llvm::raw_ostream &OS);

  /// Prints the arguments which the instruction uses in the .yaml format
  void print_uses(llvm::raw_ostream &OS);

  /// Prints the arguments which the instruction defines in the .yaml format
  void print_defs(llvm::raw_ostream &OS);

  /// Prints size in the .yaml format
  void print_size(llvm::raw_ostream &OS);

  /// Prints what the instruction affects in the .yaml format
  void print_affects(llvm::raw_ostream &OS);

  /// Prints what the instruction is affected by in the .yaml
  void print_affected(llvm::raw_ostream &OS);

  /// Prints the instruction itinerary in the .yaml format
  void print_itinerary(llvm::raw_ostream &OS);

  /// Prints the whole instruction in the .yaml format
  void print_all(llvm::raw_ostream &OS);

private:

  void print_affs(llvm::raw_ostream &OS, string name, bool memory,
                  vector<string> regs);

  void print_usedefs(llvm::raw_ostream &OS, vector<string> usedefs,
                     string name);

  void printAttribute(string name, string value, llvm::raw_ostream &OS);

  void printField(string name, string value, llvm::raw_ostream &OS);


};

#endif
