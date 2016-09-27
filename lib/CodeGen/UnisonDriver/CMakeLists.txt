add_llvm_library(LLVMUnisonDriver
  UnisonDriver.cpp
  )

add_dependencies(LLVMUnisonDriver intrinsics_gen)
