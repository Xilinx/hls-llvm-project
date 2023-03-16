SET_SOURCE_FILES_PROPERTIES( Demangle.cpp PROPERTIES COMPILE_FLAGS -std=c++17 COMPILE_FLAGS -std=c++14 )
SET_SOURCE_FILES_PROPERTIES( ItaniumDemangle.cpp PROPERTIES COMPILE_FLAGS -std=c++17 COMPILE_FLAGS -std=c++14 )

add_llvm_library(LLVMDemangle
  Demangle.cpp
  ItaniumDemangle.cpp
  MicrosoftDemangle.cpp
  MicrosoftDemangleNodes.cpp
  RustDemangle.cpp

  ADDITIONAL_HEADER_DIRS
  "${LLVM_MAIN_INCLUDE_DIR}/llvm/Demangle"

)
