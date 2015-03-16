set(LLVM_LINK_COMPONENTS ${LLVM_TARGETS_TO_BUILD} asmparser ipo bitreader bitwriter irreader)

add_llvm_tool(llvm-extract-all
  llvm-extract-all.cpp
  )
