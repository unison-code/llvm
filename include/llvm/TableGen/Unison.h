//===- llvm/TableGen/Unison.h - Unison tool ---------------------*- C++ -*-===//
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
/// This file contains the declarations of methods for extracting data needed
/// for Unison.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TABLEGEN_UNISON_H
#define LLVM_TABLEGEN_UNISON_H
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/Instruction.h"
#include <utility>

namespace llvm {

  typedef std::pair<std::string, std::string> string_pair;

  /// \brief outputs information for Unison
  ///
  /// The crucial method of this file, it prints extracted information
  /// for the Unison compiler as a valid .yaml file
  /// \param OS output stream to which it prints the .yaml file
  /// \param Records structure that holds all the information about the
  /// data which TableGen tool has
  void printUnisonFile(raw_ostream &OS, RecordKeeper &Records);

  std::vector<std::string> flat(Record *rec);

  inline void printYaml(std::vector<Instruction> instructions,
                        raw_ostream &OS);

  inline std::string getRecordItinerary(Record *rec);

  std::vector<std::string> getRegisterList (std::string field, Record *rec);

  inline bool getRecordBool (Record *rec, std::string field, bool def);

  inline int getRecordSize (Record *rec) {
    return rec->getValueAsInt("Size");
  }

  std::vector<string_pair> *parseOperands(std::string field, Record *rec);

  std::vector<std::string> getNames(std::vector<string_pair> *list);

  void execute_constraints(std::vector<string_pair> *outs, std::string cons);

  std::vector<unison::operand> getOperands(std::vector<string_pair> *outs,
                                           std::vector<string_pair> *ins,
                                           RecordKeeper &records);

  void get_operands_from_vector(std::vector<string_pair> *vec,
                                std::vector<string_pair> *help,
                                std::vector<unison::operand>* operands,
                                bool defs,
                                RecordKeeper &records);

  bool isRegister(Record *rec);

  bool isLabel(Record *rec);

  std::string getRecordType (Record *rec);

  std::string getRecordId (Record *rec);

  bool fieldExists(Record *rec, std::string field);

  bool checkIfAllNeededFieldsExist(Record *rec);

  std::vector<std::string> split(std::string s, char delimiter);

  std::string eatWhiteSpace(std::string s);

  std::string escape(std::string name);

  std::string downCase(std::string s);

}

#endif
