//===- MIRPrinter.h - MIR serialization format printer --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the functions that print out the LLVM IR and the machine
// functions using the MIR serialization format.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_CODEGEN_MIRPRINTER_H
#define LLVM_LIB_CODEGEN_MIRPRINTER_H

#include "SpillPlacement.h"

namespace llvm {

class MachineFunction;
class Module;
class raw_ostream;

/// This structure contains some auxiliary information necessary to print the
/// MIR code.
struct MIRAuxiliaryInfo {
  bool UnisonStyle;
  const SpillPlacement * SP;
};

/// Print LLVM IR using the MIR serialization format to the given output stream.
void printMIR(raw_ostream &OS, const Module &M);

/// Print a machine function using the MIR serialization format to the given
/// output stream.
 void printMIR(raw_ostream &OS, const MachineFunction &MF,
               MIRAuxiliaryInfo & info);

} // end namespace llvm

#endif
