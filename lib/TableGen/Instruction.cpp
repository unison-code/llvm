//===- Instruction.cpp - Instruction Implementation -------------*- C++ -*-===//
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
/// This file contains the Implementation of the Instruction class.
//
//===----------------------------------------------------------------------===//

#include<cstdio>
#include<vector>
#include<string>
#include<iostream>
#include<iomanip>
#include<sstream>
#include"llvm/TableGen/Instruction.h"

using namespace std;

Instruction::Instruction(string id, string type,
                         vector<unison::operand> operands,
                         vector<string> uses,
                         vector<string> defs, int size, bool affects_mem,
                         bool affected_mem,
                         vector<string> affects_reg,
                         vector<string> affected_reg, string itinerary) {
  this->id = id;
  this->type = type;
  this->operands = operands;
  this->uses = uses;
  this->defs = defs;
  this->size = size;
  this->affects_mem = affects_mem;
  this->affected_mem = affected_mem;
  this->affects_reg = affects_reg;
  this->affected_reg = affected_reg;
  this->itinerary = itinerary;
}

void Instruction::print_id(llvm::raw_ostream &OS) {
  stringstream buffer;
  buffer << setw(8) << " " << setw(22) << left << "- id:"
         << id.c_str() << endl;
  OS << buffer.str();
}

void Instruction::print_type(llvm::raw_ostream &OS) {
  printAttribute("type:", type.c_str(), OS);
}

void Instruction::print_operands(llvm::raw_ostream &OS) {
  stringstream buffer;
  buffer << setw(10) << " " << "operands:" << endl;
  OS << buffer.str();
  for (unison::operand op : operands) {
    string value;
    if (op.label) {
      value = "label";
    } else if (op.bound) {
      value = "bound";
    } else {
      value = "[register, " + op.usedef_field + ", " +
        op.regtype_field + "]";
    }
    printField(op.name, value, OS);
  }
}


void Instruction::print_usedefs(llvm::raw_ostream &OS,
                                vector<string> usedefs, string name) {
  string value = "";
  for (unsigned int i = 0; i < usedefs.size(); i++) {
    if (i != 0) {
      value += ", ";
    }
    value += usedefs[i];
  }
  value = "[" + value + "]";
  printAttribute(name + ":", value.c_str(), OS);
}

void Instruction::print_uses(llvm::raw_ostream &OS) {
  print_usedefs(OS, uses, "uses");
}

void Instruction::print_defs(llvm::raw_ostream &OS) {
  print_usedefs(OS, defs, "defines");
}

void Instruction::print_size(llvm::raw_ostream &OS) {
  printAttribute("size:", to_string(size).c_str(), OS);
}


void Instruction::print_affects(llvm::raw_ostream &OS) {
  print_affs(OS, "affects", affects_mem, affects_reg);
}

void Instruction::print_affected(llvm::raw_ostream &OS) {
  print_affs(OS, "affected-by", affected_mem, affected_reg);
}

void Instruction::print_affs(llvm::raw_ostream &OS, string name,
                             bool memory, vector<string> regs) {
  stringstream buffer;
  buffer << setw(10) << " " << name + ":" << endl;
  OS << buffer.str();

  if (memory) {
    printField("mem", "memory", OS);
  }
  for (string s : regs) {
    printField(s, "register", OS);
  }
}

void Instruction::print_itinerary(llvm::raw_ostream &OS) {
  printAttribute("itinerary:", itinerary.c_str(), OS);
}

void Instruction::print_all(llvm::raw_ostream &OS) {
  OS << "\n";
  Instruction::print_id(OS);
  Instruction::print_type(OS);
  Instruction::print_operands(OS);
  Instruction::print_uses(OS);
  Instruction::print_defs(OS);
  Instruction::print_size(OS);
  Instruction::print_affects(OS);
  Instruction::print_affected(OS);
  Instruction::print_itinerary(OS);
}

/// prints a simple attribute
void Instruction::printAttribute(string name, string value,
                                 llvm::raw_ostream &OS) {
  stringstream buffer;
  if (value == "") {
    buffer << setw(10) << " " << name << endl;
  } else {
    buffer << setw(10) << " " << setw(20) << left << name
           << value << endl;
  }
  OS << buffer.str();
}

/// prints the subelements of an complex attribute
void Instruction::printField(string name, string value,
                             llvm::raw_ostream &OS) {
  string name1 = "- " + name + ": ";
  stringstream buffer1;
  buffer1 << setw(11) << " " << setw(19) << left << name1.c_str()
          << value.c_str() << endl;
  OS << buffer1.str();
}
