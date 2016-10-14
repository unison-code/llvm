//===- Unison.cpp - Unison tool implementation ----------------------------===//
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
/// Implementation of the unison tool which extracts necessary information for
/// the Unison compiler.
//
//===----------------------------------------------------------------------===//

#include<iostream>
#include<sstream>
#include<iomanip>
#include<ctype.h>
#include"llvm/TableGen/Unison.h"
#include"llvm/TableGen/Instruction.h"

namespace llvm {

  /// names of the classes which suggest the object of that class is a register
  std::vector<std::string> registerNames {"RegisterClass", "Register",
      "RegisterOperand", "RegisterTuples"};


  void printUnisonFile(raw_ostream &OS, RecordKeeper &records) {
    std::vector<Instruction> instructions;
    for (const auto & D : records.getDefs()) {

      Record * rec = &(*D.second);

      if (!checkIfAllNeededFieldsExist(rec)) continue;

      std::string id = getRecordId(rec);
      std::string type = getRecordType(rec);

      // tricky part
      std::vector<string_pair> *out_list = parseOperands("OutOperandList", rec);
      std::vector<string_pair> *in_list =  parseOperands("InOperandList", rec);
      execute_constraints(out_list, rec->getValueAsString("Constraints"));
      std::vector<std::string> uses = getNames(in_list);
      std::vector<std::string> defs = getNames(out_list);
      std::vector<unison::operand> operands =
        getOperands(out_list, in_list, records);

      int size = getRecordSize(rec);
      bool affects_mem = getRecordBool(rec, "mayStore", false);
      bool affected_mem = getRecordBool(rec, "mayLoad", false);
      std::vector<std::string> affects_reg = getRegisterList("Defs", rec);
      std::vector<std::string> affected_reg = getRegisterList("Uses", rec);
      std::string itinerary = getRecordItinerary(rec);

      Instruction ins(id, type, operands, uses, defs, size, affects_mem,
                      affected_mem, affects_reg, affected_reg, itinerary);
      instructions.push_back(ins);
    }
    printYaml(instructions, OS);

  }

  /// printing of the instructions to the \p OS in .yaml format
  inline void printYaml(std::vector<Instruction> instructions, raw_ostream &OS) {
    OS << "---\ninstruction-set:\n\n";
    std::stringstream buffer;
    buffer << std::setw(3) << " " << "- group: " << "allInstructions" << "\n";
    OS << buffer.str();
    std::stringstream buffer1;
    buffer1 << std::setw(5) << " " << "instructions:" << "\n\n";
    OS << buffer1.str();
    for (Instruction in : instructions) {
      in.print_all(OS);
    }
  }

  /// Returns a vector of register names extraced from a \p field attribute
  /// of the given Record \p rec.
  /// if the \p field is not a list it will result in an exception
  std::vector<std::string> getRegisterList (std::string field, Record *rec) {
    ListInit *list = rec->getValueAsListInit(field);
    std::vector<std::string> regs;
    for (int i = 0, k = list->size(); i < k; i++) {
      regs.push_back(escape(list->getElement(i)->getAsString()));
    }
    return regs;
  }

  /// Gets the Itinerary name of the given record
  inline std::string getRecordItinerary(Record *rec) {
    Record *def = rec->getValueAsDef("Itinerary");
    return def->getName();
  }

  /// Gets the boolean value of the given \p field in the given record
  /// \p rec and it it is not set, then returns the given default value
  /// \p def
  inline bool getRecordBool (Record *rec, std::string field, bool def) {
    bool unset = false;
    bool val = rec->getValueAsBitOrUnset(field, unset);
    return unset ? def : val;
  }

  /// Gets operands of the given field from the record
  /// makes pairs <type, name> where type detemines the type of the register,
  /// or immediate value, or label
  /// and name is the name that is given to that register/value/label
  /// (like src1, ...)
  std::vector<string_pair> *parseOperands(std::string field, Record *rec) {
    DagInit *dag = rec->getValueAsDag(field);
    std::vector<string_pair> *ret = new std::vector<string_pair>;
    for (int i = 0, k = dag->getNumArgs(); i < k; i++) {
      DefInit *def = (DefInit *) dag->getArg(i);
      std::vector<std::string> types = flat(def->getDef());
      for (int j = 0, k = types.size(); j < k; j++) {
        std::string type = types[j];
        std::string name = types.size() == 1 ? dag->getArgName(i) :
          (dag->getArgName(i) + std::to_string(j+1));
        if (name == "") name = "unnamed";
        string_pair pr(type, escape(name));
        ret->push_back(pr);
      }
    }
    return ret;
  }

  /// Extracts all suboperands of an operand, if such exist, and returns their
  /// names in a list.
  /// If they don't, just returns the name of the operand as a list of one element
  std::vector<std::string> flat(Record *rec) {
    std::vector<std::string> ret;
    RecordVal *field = rec->getValue("MIOperandInfo");
    if (field == nullptr) {
      ret.push_back(rec->getNameInitAsString());
      return ret;
    }
    DagInit *dag = (DagInit*) field->getValue();
    if (dag->getNumArgs() == 0) {
      ret.push_back(rec->getNameInitAsString());
      return ret;
    }
    for (int i = 0, k = dag->getNumArgs(); i < k; i++) {
      DefInit *def = (DefInit *) dag->getArg(i);
      std::vector<std::string> subs = flat(def->getDef());
      ret.insert(ret.end(), subs.begin(), subs.end());
    }
    return ret;
  }

  /// Returns only the names found in the given list of <type, name>
  std::vector<std::string> getNames(std::vector<string_pair> *list) {
    std::vector<std::string> names;
    for (string_pair pp : *list) {
      names.push_back(pp.second);
    }
    return names;
  }

  /// Executes given constraints, meaning replaces names with alias names
  void execute_constraints(std::vector<string_pair> *outs, std::string cons) {
    if (cons.size() == 0) return;
    for (std::string con : split(cons, ',')) {
      std::string con0 = eatWhiteSpace(con);
      if (con0.find("@earlyclobber") == 0) continue;
      std::vector<std::string> list = split(con0, '=');
      assert(list.size() == 2);
      std::string first = escape(eatWhiteSpace(list[0]).substr(1));
      std::string second = escape(eatWhiteSpace(list[1]).substr(1));
      for (unsigned int i = 0; i < outs->size(); i++) {
        if ((*outs)[i].second == first) {
          (*outs)[i].second = second;
        }
        else if ((*outs)[i].second == second) {
          (*outs)[i].second = first;
        }
      }
    }
  }

  /// constructs a list of full list of operands, from given input operands
  /// and output operands
  std::vector<unison::operand> getOperands(std::vector<string_pair> *outs,
                                           std::vector<string_pair> *ins,
                                           RecordKeeper &records) {
    std::vector<unison::operand> operands;
    get_operands_from_vector(outs, ins, &operands, true, records);
    get_operands_from_vector(ins, outs, &operands, false, records);
    return operands;
  }

  /// Adds operands from the \p vec list of operands to the \p operand list
  void get_operands_from_vector(std::vector<string_pair> *vec,
                                std::vector<string_pair> *help,
                                std::vector<unison::operand>* operands,
                                bool defs,
                                RecordKeeper &records) {
    for(string_pair pr : *vec) {
      unison::operand *op = new unison::operand;
      op->name = pr.second;

      bool flag = false;
      for (unsigned int i = 0; i < operands->size(); i++) {
        if ((*operands)[i].name == op->name) {
          flag = true;
          delete op;
          break;
        }
      }
      if (flag) continue;

      std::string usedef_f = defs ? "def" : "use";
      if (std::find(help->begin(), help->end(), pr) != help->end()) {
        if (defs) usedef_f = "use" + usedef_f;
        else usedef_f = usedef_f + "def";
      }
      op->usedef_field = usedef_f;
      op->regtype_field = pr.first;

      // code for labels, bounds, registers here
      Record *def = records.getDef(op->regtype_field);

      if (isRegister(def)) {
        op->label = false;
        op->bound = false;
      }
      else if (isLabel(def)) {
        op->label = true;
        op->bound = false;
      }
      else {
        op->label = false;
        op->bound = true;
      }
      operands->push_back(*op);
    }
  }

  bool isLabel(Record *rec) {
    RecordVal *val = rec->getValue("Type");      // gets ValueType
    if (val == nullptr) return false;
    DefInit *def = (DefInit*) val->getValue();
    return def->getAsString() == "OtherVT"; // supposedly the mark for the label
  }

  bool isRegister(Record *rec) {
    if (rec == nullptr) return false;
    std::vector<Record*> super = rec->getSuperClasses();
    for (int i = 0, k = super.size(); i < k; i++) {
      std::string name = super[i]->getName();
      for (int j = 0, l = registerNames.size(); j < l; j++) {
        if (name == registerNames[j]) {
          return true;
        }
      }
    }
    return false;
  }

  /// Returns the string the describes the type of the recrod as "call",
  /// "linear" or "branch".
  std::string getRecordType (Record *rec) {
    if (getRecordBool(rec, "isCall", false)) return "call";
    if (getRecordBool(rec, "isBranch", false) ||
        getRecordBool(rec, "isReturn", false)) return "branch";
    return "linear";
  }

  inline std::string getRecordId (Record *rec) {
    return rec->getName();
  }

  /// Function which cheks whether all attributes of the given record \p rec are
  /// present in order for the record to be analyzed as a instruction
  bool checkIfAllNeededFieldsExist(Record *rec) {
    if (!fieldExists(rec, "isCall") || !fieldExists(rec, "isBranch")
        ||!fieldExists(rec, "Constraints")
        || !fieldExists(rec, "OutOperandList")
        || !fieldExists(rec, "InOperandList")
        || !fieldExists(rec, "Size")
        || !fieldExists(rec, "mayLoad") ||  !fieldExists(rec, "mayStore")
        || !fieldExists(rec, "Itinerary") || !fieldExists(rec, "isReturn")
        || !fieldExists(rec, "Uses")
        || !fieldExists(rec, "Defs")) {
      return false;
    }
    return true;
  }

  /// Function which checks whether a given attribute \p field exists in the
  /// given record \p rec
  bool fieldExists(Record *rec, std::string field) {
    RecordVal *val = rec->getValue(field);
    return val != nullptr;
  }

  /// Splits the string \p s with \p delimiter and returns a vector of strings.
  std::vector<std::string> split(std::string s, char delimiter) {
    std::stringstream stream(s);
    std::string element;
    std::vector<string> ret;
    while (getline(stream, element, delimiter)) {
      ret.push_back(element);
    }
    return ret;
  }

  /// Function which deletes whitespace at the beginning and at the end of the
  /// given string and return the result, which is a new string
  string eatWhiteSpace(std::string s) {
    for(unsigned int i = 0; i < s.size(); i++) {
      if (s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == '\r') {
        s.erase(i, 1);
        i--;
      }
      else break;
    }
    for(int i = s.size() - 1; i >= 0; i--) {
      if (s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == '\r') {
        s.erase(i, 1);
      }
      else break;
    }
    return s;
  }

  /// Function that escapes the given string for yamls, so that it
  /// isn't marked as a keyword for yaml.
  std::string escape(std::string name) {
    if (downCase(name) == "true" || downCase(name) == "false"
        || downCase(name) == "n"
        || downCase(name) == "y" || downCase(name) == "yes"
        || downCase(name) == "no"
        || downCase(name) == "on" || downCase(name) == "off")
      return name + "'";
    return name;
  }

  /// Down cases the given string.
  std::string downCase(std::string s) {
    string s1 = "";
    for (unsigned int i = 0; i < s.size(); i++) {
      s1 += std::tolower(s[i]);
    }
    return s1;
  }



}
