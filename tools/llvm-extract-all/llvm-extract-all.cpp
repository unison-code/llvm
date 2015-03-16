//===- llvm-extract.cpp - LLVM function extraction utility ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This utility generates one module for each non-empty function in the input
// module.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/LLVMContext.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/CodeGen/LinkAllCodegenComponents.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Transforms/IPO.h"
#include <memory>
using namespace llvm;

// InputFilename - The filename to read from.
static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode file>"),
              cl::init("-"), cl::value_desc("filename"));

static cl::opt<bool>
Force("f", cl::desc("Enable binary output on terminals"));

static cl::opt<bool>
PrependModule("m", cl::desc("Prepend module name"));

static cl::opt<bool>
OutputAssembly("S",
               cl::desc("Write output as LLVM assembly"), cl::Hidden);

static cl::opt<bool>
DisableStripDeadDebugInfo("d",
                          cl::desc("Disable strip of dead debug info"),
                          cl::Hidden);

std::string fileName (const std::string& str)
{
  size_t pos, pos2;
  pos = str.find_last_of("/\\");
  std::string name(str.substr(pos + 1));
  pos2 = name.find(".ll");
  return name.substr(0, pos2);
}


int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);

  LLVMContext &Context = getGlobalContext();
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  cl::ParseCommandLineOptions(argc, argv,
                              "llvm extractor of all non-empty functions\n");

  // Use lazy loading, since we only care about selected global values.
  SMDiagnostic Err;
  OwningPtr<Module> CM;
  CM.reset(getLazyIRFileModule(InputFilename, Err, Context));

  if (CM.get() == 0) {
    Err.print(argv[0], errs());
    return 1;
  }

  for (Module::iterator F = CM->begin(); F != CM->end(); F++) {

    std::auto_ptr<Module> M;
    M.reset(getLazyIRFileModule(InputFilename, Err, Context));

    // Use SetVector to avoid duplicates.
    SetVector<GlobalValue *> GVs;

    Function *GV = M->getFunction(F->getName());

    if (GV->empty()) continue;

    errs() << "Extracting " << F->getName() << " from " <<
      M->getModuleIdentifier() << "...\n";

    GVs.insert(&*GV);

    // Materialize requisite global values.
    for (size_t i = 0, e = GVs.size(); i != e; ++i) {
      GlobalValue *GV = GVs[i];
      if (GV->isMaterializable()) {
        std::string ErrInfo;
        if (GV->Materialize(&ErrInfo)) {
          errs() << argv[0] << ": error reading input: " << ErrInfo << "\n";
          return 1;
        }
      }
    }

    // In addition to deleting all other functions, we also want to spiff it
    // up a little bit.  Do this now.
    PassManager Passes;

    std::vector<GlobalValue*> Gvs(GVs.begin(), GVs.end());

    Passes.add(createGVExtractionPass(Gvs, false));
    Passes.add(createGlobalDCEPass());           // Delete unreachable globals
    if (!DisableStripDeadDebugInfo) {
      Passes.add(createStripDeadDebugInfoPass());    // Remove dead debug info
    }
    Passes.add(createStripDeadPrototypesPass());   // Remove dead func decls

    std::string OutputFilename;
    if (PrependModule) {
      OutputFilename += fileName(M->getModuleIdentifier());
      OutputFilename += ".";
    }
    OutputFilename += F->getName();
    OutputFilename += ".ll";

    std::string ErrorInfo;
    tool_output_file Out(OutputFilename.c_str(), ErrorInfo, sys::fs::F_Binary);
    if (!ErrorInfo.empty()) {
      errs() << ErrorInfo << '\n';
      return 1;
    }

    if (OutputAssembly)
      Passes.add(createPrintModulePass(&Out.os()));
    else if (Force || !CheckBitcodeOutputToConsole(Out.os(), true))
      Passes.add(createBitcodeWriterPass(Out.os()));

    Passes.run(*M.get());

    // Declare success.
    Out.keep();
  }
  return 0;
}
