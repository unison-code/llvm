//===-- UnisonDriver.cpp - Machine IR emission pass. -----------------===//
//
//  Main authors:
//    Roberto Castaneda Lozano <rcas@sics.se>
//
//  Contributing authors:
//    Mattias Eriksson <mattias.v.eriksson@ericsson.com>
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
// This file replaces LLVM's register allocation and instruction scheduling
// with the Unison pipeline. In order for this pass to work properly,
// the following executables/scripts MUST be on the PATH:
//  - uni (Unison)
//  - gecode-presolver
//  - gecode-solver
//
// The pass assumes that it is run right before emission and gets as input
// the name of a MIR file that has been generated with the input to Unison
// (see Passes.cpp).
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "unison"
#include "llvm/CodeGen/UnisonDriver/UnisonDriver.h"

using namespace llvm;

static cl::opt<bool> UnisonVerbose("unison-verbose",
    cl::Optional,
    cl::desc("Show Unison command lines and process output"),
    cl::init(false));

static cl::opt<unsigned int> UnisonMaxBlockSize("unison-maxblocksize",
    cl::Optional,
    cl::desc("--maxblocksize parameter passes to Unison import"),
    cl::init(25));

static cl::opt<unsigned int> UnisonPresolveTimeout("unison-ps-timeout",
    cl::Optional,
    cl::desc("Unison presolver timeout in seconds"),
    cl::init(180));

static cl::opt<bool> UnisonNoClean("unison-no-clean",
    cl::Optional,
    cl::desc("Do not clean Unison temporary files"),
    cl::init(false));

static cl::opt<bool> UnisonLint("unison-lint",
    cl::Optional,
    cl::desc("Run Unison lint on the output of every Unison command (for debugging purposes)"),
    cl::init(false));

static cl::opt<std::string> UnisonImportFlags("unison-import-flags",
    cl::Optional,
    cl::desc("'uni import' flags"),
    cl::init(""));

static cl::opt<std::string> UnisonLinearizeFlags("unison-linearize-flags",
    cl::Optional,
    cl::desc("'uni linearize' flags"),
    cl::init(""));

static cl::opt<std::string> UnisonExtendFlags("unison-extend-flags",
    cl::Optional,
    cl::desc("'uni extend' flags"),
    cl::init(""));

static cl::opt<std::string> UnisonAugmentFlags("unison-augment-flags",
    cl::Optional,
    cl::desc("'uni augment' flags"),
    cl::init(""));

static cl::opt<std::string> UnisonNormalizeFlags("unison-normalize-flags",
    cl::Optional,
    cl::desc("'uni normalize' flags"),
    cl::init(""));

static cl::opt<std::string> UnisonModelFlags("unison-model-flags",
    cl::Optional,
    cl::desc("'uni model' flags"),
    cl::init(""));

static cl::opt<std::string> UnisonPresolverFlags("unison-presolver-flags",
    cl::Optional,
    cl::desc("Unison presolver flags"),
    cl::init(""));

static cl::opt<std::string> UnisonSolverFlags("unison-solver-flags",
    cl::Optional,
    cl::desc("Unison solver flags"),
    cl::init(""));

static cl::opt<std::string> UnisonExportFlags("unison-export-flags",
    cl::Optional,
    cl::desc("'uni export' flags"),
    cl::init(""));

UnisonDriver::UnisonDriver() : MachineFunctionPass(ID), PreMir("") {}

extern cl::opt<std::string> UnisonSingleFunction;

UnisonDriver::UnisonDriver(StringRef Pre) :
  MachineFunctionPass(ID),
  PreMir(Pre)
 {
  initializeUnisonDriverPass(*PassRegistry::getPassRegistry());
  initializeSpillPlacementPass(*PassRegistry::getPassRegistry());
}

UnisonDriver::~UnisonDriver() {
  if (!UnisonNoClean) {
    sys::fs::remove(Twine(PreMir), true);
    sys::fs::remove(Twine(AsmMir), true);
  }
}

void UnisonDriver::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequiredID(SpillPlacementID);
  MachineFunctionPass::getAnalysisUsage(AU);
}

bool UnisonDriver::runOnMachineFunction(MachineFunction &MF) {
  const TargetMachine *TM = &MF.getTarget();

  // Run Unison for MF if either -unison is used or the function is
  // annotated with __attribute__((annotate("unison")))
  const Function *F = MF.getFunction();
  if (!(TM->Options.Unison || hasUnisonAnnotation(F->getParent(), F))) {
    cleanPaths();
    return false;
  }

  // Using -unison-single-function on the command line overrides the other ways
  // of activating Unison.
  const StringRef &UnisonSingleFunctionStr = UnisonSingleFunction;
  if (!UnisonSingleFunctionStr.equals(StringRef("")) &&
      !MF.getName().equals(UnisonSingleFunction)) {
    cleanPaths();
    return false;
  }
  // Load Unison paths only if we are really going to use them
  UnisonPath.load("uni");
  PresolverPath.load("gecode-presolver");
  SolverPath.load("gecode-solver");

  std::string TargetName;
  if (TM->getTargetTriple().getArch() == Triple::hexagon &&
      (TM->getTargetCPU() == "hexagonv4")) {
    TargetName = "Hexagon";
  } else if (TM->getTargetTriple().getArch() == Triple::arm &&
             TM->getTargetCPU() == "arm1156t2f-s") {
    TargetName = "ARM";
  } else {
    report_fatal_error("Target unavailable in Unison", false);
  }
  Target = "--target="+TargetName;

  // 0. Create baseline *.asm.mir

  AsmMir = makeTempFile("asm.mir");
  std::error_code EC;
  raw_fd_ostream OS(AsmMir, EC, sys::fs::F_RW);
  FunctionPass *MIRE = createPrintMIRPass(OS, true, true);
  MIRE->setResolver(this->getResolver());
  MIRE->runOnFunction(*const_cast<Function*>(F));
  OS.flush();
  OS.close();

  // 1. Import: *.mir --> *.uni

  std::string Uni = makeTempFile("uni");

  std::string Goal = MF.getFunction()->optForSize() ? "size" : "speed";
  std::vector<std::string> ImportArgv =
    { "--function=" + MF.getName().str(),
      "--maxblocksize=" + std::to_string(UnisonMaxBlockSize),
      "--goal=" + Goal};
  insertFlags(ImportArgv, UnisonImportFlags, UnisonLint);

  ensure(runTool("import", PreMir, Uni, ImportArgv),
         "'uni import' failed.");

  // 2. Linearize: *.uni --> *.lssa.uni

  std::string Lssa = makeTempFile("lssa.uni");

  std::vector<std::string> LinearizeArgv;
  insertFlags(LinearizeArgv, UnisonLinearizeFlags, UnisonLint);

  ensure(runTool("linearize", Uni, Lssa, LinearizeArgv),
         "'uni linearize' failed.");

  // 3. Extend: *.lssa.uni --> *.ext.uni

  std::string Ext = makeTempFile("ext.uni");

  std::vector<std::string> ExtendArgv;
  insertFlags(ExtendArgv, UnisonExtendFlags, UnisonLint);

  ensure(runTool("extend", Lssa, Ext, ExtendArgv),
         "'uni extend' failed.");

  // 4. Augment: *.ext.uni --> *.alt.uni

  std::string Alt = makeTempFile("alt.uni");

  std::vector<std::string> AugmentArgv;
  insertFlags(AugmentArgv, UnisonAugmentFlags, UnisonLint);

  ensure(runTool("augment", Ext, Alt, AugmentArgv),
         "'uni augment' failed.");

  // 5. Normalize: *.asm.mir --> *.llvm.mir

  std::string LlvmMir = makeTempFile("llvm.mir");

  std::vector<std::string> NormalizeArgv;
  insertFlags(NormalizeArgv, UnisonNormalizeFlags);

  ensure(runTool("normalize", AsmMir, LlvmMir, NormalizeArgv),
         "'uni normalize' failed.");

  // 6. Model: *.alt.uni, *.llvm.mir --> *.json

  std::string Json = makeTempFile("json");

  std::vector<std::string> ModelArgv =
    {"--basefile=" + LlvmMir, "+RTS", "-K20M", "-RTS" };
  insertFlags(ModelArgv, UnisonModelFlags);

  ensure(runTool("model", Alt, Json, ModelArgv),
         "'uni model' failed.");

  // 7. Presolver: *.json --> *.ext.json

  std::string ExtJson = makeTempFile("ext.json");

  std::vector<std::string> PresolverArgv =
    { "gecode-presolver", "-o", ExtJson };
  insertFlags(PresolverArgv, UnisonPresolverFlags);
  PresolverArgv.push_back(Json);

  Command PresolverCmd(PresolverPath, PresolverArgv);
  ensure(PresolverCmd.run(), "'gecode-presolver' failed.");

  // 8. Solver: *.ext.json --> *.out.json

  std::string OutJson = makeTempFile("out.json");

  std::vector<std::string> SolverArgv =
    { "gecode-solver", "-o", OutJson, "--verbose" };
  insertFlags(SolverArgv, UnisonSolverFlags);
  SolverArgv.push_back(ExtJson);

  Command solver_cmd(SolverPath, SolverArgv);
  ensure(solver_cmd.run(), "'gecode-solver' failed.");

  // 9. Export: *.alt.uni, *.out.json, *.llvm.mir --> *.unison.mir

  std::string Unisonmir = makeTempFile("unison.mir");

  std::vector<std::string> ExportArgv =
    { "--basefile=" + LlvmMir, "--solfile=" + OutJson };
  insertFlags(ExportArgv, UnisonExportFlags);

  ensure(runTool("export", Alt, Unisonmir, ExportArgv),
         "'uni export' failed.");

  // 10. Load *.unison.mir

  while(MF.begin() != MF.end()) {
    MF.erase(MF.begin());
  }

  MF.RenumberBlocks(nullptr);

  LLVMContext &Context = getGlobalContext();
  SMDiagnostic ErrDiag;
  std::unique_ptr<Module> M;
  std::unique_ptr<MIRParser> MIR;
  MIR = createMIRParserFromFile(Unisonmir, ErrDiag, Context);
  if (MIR) {
    M = MIR->parseLLVMModule();
    assert(M && "parseLLVMModule should exit on failure");
  }
  MIR->initializeMachineFunction(MF);

  cleanPaths();
  return true;
}

/// Returns true if function \p F has a Unison annotation. If F is nullptr,
/// returns true if any function in \p M is Unison annotated.
bool UnisonDriver::hasUnisonAnnotation(const Module *M,
                                       const Function *F) {
  const auto As = M->getNamedGlobal("llvm.global.annotations");
  if (!As)
    return false;

  const auto A = cast<ConstantArray>(As->getOperand(0));
  for (unsigned int I = 0; I < A->getNumOperands(); I++) {
    const auto E = cast<ConstantStruct>(A->getOperand(I));
    const auto Fn = dyn_cast<Function>(E->getOperand(0)->getOperand(0));
    if (F && F != Fn)
      continue;

    const auto GV = dyn_cast<GlobalVariable>(E->getOperand(1)->getOperand(0));
    if (!GV)
      continue;

    const StringRef &An =
      dyn_cast<ConstantDataArray>(GV->getOperand(0))->getAsCString();

    if (An.equals(StringRef("unison")))
      return true;
  }
  return false;
}

void UnisonDriver::ensure(bool res, const char* msg) {
  if (!res) {
    cleanPaths();
    report_fatal_error(msg, false);
  }
}

std::string UnisonDriver::makeTempFile(const char* Suffix) {
  SmallString<128> rpath;
  std::error_code errc =
    sys::fs::createTemporaryFile(Twine("unison"), StringRef(Suffix), rpath);
  if (errc) {
    errs() << "Failed to create temporary file!\n";
    std::abort();
  }
  std::string spath(rpath.c_str());
  TempPaths.push_back(spath);
  return spath;
}

bool UnisonDriver::runTool(const char* tool, std::string input,
                           std::string output,
                           std::vector<std::string> & extra) {
  std::vector<std::string> args =
    { "uni", tool, Target, input, "-o", output };
  for (auto arg : extra) {
    args.push_back(arg);
  }
  Command cmd(UnisonPath, args);
  return cmd.run();
}

void UnisonDriver::cleanPaths() {
  if (UnisonNoClean)
    return;
  for (auto I = TempPaths.begin(), E = TempPaths.end(); I != E; I++) {
    std::error_code errc = sys::fs::remove(Twine(*I), false);
    if (errc) {
      errs() << "Temporary file (" << *I << ") could not be removed!\n";
    }
  }
  TempPaths.clear();
}

void UnisonDriver::insertFlags(std::vector<std::string> & argv,
                               std::string & flags, bool lintFlag) {
  std::string flag;
  std::stringstream s(flags);
  while (s >> flag) {
    argv.push_back(flag);
  }
  if (lintFlag)
    argv.push_back("--lint");
}

char UnisonDriver::ID = 0;

INITIALIZE_PASS(UnisonDriver, "unison-driver", "Unison driver", false, false)

void UnisonDriver::ProgramPath::load(const char* pgm) {
  ErrorOr<std::string> result = sys::findProgramByName(pgm);
  if (result) {
    m_path = result.get();
  }
  else { // Report (bad) result and exit.
    report_fatal_error("Program \'" + std::string(pgm) + "\' not found", false);
  }
}

UnisonDriver::Command::Command(ProgramPath &pgm, std::vector<std::string> argv)
  : m_cmd(pgm.getPath()),
    m_args(argv),
    m_sink("") {
  m_redir[0] = nullptr;
  m_redir[1] = UnisonVerbose ? nullptr : &m_sink;
  m_redir[2] = m_redir[1];
}

bool UnisonDriver::Command::run() {
  if (UnisonVerbose) print();
  // Unpack string vector to plain array of char*.
  unsigned argc = m_args.size();
  const char** argv = new (std::nothrow) const char* [argc + 1];
  if (argv == nullptr) return false;
  for (unsigned i = 0; i < argc; i++) {
    argv[i] = m_args[i].c_str();
  }
  argv[argc] = nullptr;   // Terminating 'null' required.
  std::string msg = "<no-message>";
  int rcode =
    sys::ExecuteAndWait(StringRef(m_cmd), argv, nullptr, m_redir, 0, 0, &msg);
  delete[] argv;
  if (rcode != 0) {
    errs() << "*** " << msg << " ***\n";
  }
  return rcode == 0;
}

void UnisonDriver::Command::print() {
  std::vector<std::string>::iterator end = m_args.end();
  errs() << m_cmd << ':';
  for (auto itr = m_args.begin(); itr != end; ++itr) {
    errs() << ' ' << *itr;
  }
  errs() << '\n';
}
