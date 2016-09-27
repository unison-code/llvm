add_llvm_library(LLVMTableGen
  Error.cpp
  Instruction.cpp
  Main.cpp
  Record.cpp
  SetTheory.cpp
  StringMatcher.cpp
  TableGenBackend.cpp
  TGLexer.cpp
  TGParser.cpp
  Unison.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/TableGen
  )
