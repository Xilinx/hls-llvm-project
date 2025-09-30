# (C) Copyright 2016-2022 Xilinx, Inc.
# (C) Copyright 2023-2025 Advanced Micro Devices, Inc.
add_library(sqlite3 SHARED IMPORTED)

add_library(fpo_v7_1_gmp SHARED IMPORTED)
add_library(fpo_v7_1_mpfr SHARED IMPORTED)
add_library(fpo_v7_1 SHARED IMPORTED)

# Find out position of libsqlite3 .so/.lib/.dll
#################################################
if (DEFINED LINK_DIRS)
  #if( MSVC OR MINGW )
  if(WIN32 OR CYGWIN)
    foreach(dir ${LINK_DIRS})
      message(STATUS dir=${dir})
      if (EXISTS "${dir}/libsqlite3.45.1.lib")
        set(sqlite_res_path_lib "${dir}/libsqlite3.45.1.lib" CACHE FILEPATH "" FORCE)
        message(STATUS sqlite_res_path_lib=${sqlite_res_path_lib})
        break()
      endif()
    endforeach()
  else()
    foreach(dir ${LINK_DIRS})
      message(STATUS dir=${dir})
      if (EXISTS "${dir}/libsqlite3.45.1.so")
        set(sqlite_res_path_so "${dir}/libsqlite3.45.1.so" CACHE FILEPATH "" FORCE)
        message(STATUS sqlite_res_path_so=${sqlite_res_path_so})
        break()
      endif()
    endforeach()
  endif()
endif()
#################################################

# Find out position of libIp_floating_point_v7_1_bitacc_cmodel .so/.lib/.dll
#################################################
if (WIN32 OR CYGWIN)
  if (EXISTS "${HLS_FPO_DIR}/libIp_floating_point_v7_1_bitacc_cmodel.lib")
    set(fpo_res_path_lib "${HLS_FPO_DIR}/libIp_floating_point_v7_1_bitacc_cmodel.lib" CACHE FILEPATH "" FORCE)
    message(STATUS fpo_res_path_lib=${fpo_res_path_lib})
  endif()
  if (EXISTS "${HLS_FPO_DIR}/libIp_floating_point_v7_1_bitacc_cmodel.dll")
    set(fpo_res_path_dll "${HLS_FPO_DIR}/libIp_floating_point_v7_1_bitacc_cmodel.dll" CACHE FILEPATH "" FORCE)
    message(STATUS fpo_res_path_dll=${fpo_res_path_dll})
  endif()
  if (EXISTS "${HLS_FPO_DIR}/libgmp.lib")
    set(gmp_res_path_lib "${HLS_FPO_DIR}/libgmp.lib" CACHE FILEPATH "" FORCE)
    message(STATUS gmp_res_path_lib=${gmp_res_path_lib})
  endif()
  if (EXISTS "${HLS_FPO_DIR}/libgmp.dll")
    set(gmp_res_path_dll "${HLS_FPO_DIR}/libgmp.dll" CACHE FILEPATH "" FORCE)
    message(STATUS gmp_res_path_dll=${gmp_res_path_dll})
  endif()
  if (EXISTS "${HLS_FPO_DIR}/libmpfr.lib")
    set(mpfr_res_path_lib "${HLS_FPO_DIR}/libmpfr.lib" CACHE FILEPATH "" FORCE)
    message(STATUS mpfr_res_path_lib=${mpfr_res_path_lib})
  endif()
  if (EXISTS "${HLS_FPO_DIR}/libmpfr.dll")
    set(mpfr_res_path_dll "${HLS_FPO_DIR}/libmpfr.dll" CACHE FILEPATH "" FORCE)
    message(STATUS mpfr_res_path_dll=${mpfr_res_path_dll})
  endif()
else()
  if (EXISTS "${HLS_FPO_DIR}/libIp_floating_point_v7_1_bitacc_cmodel.so")
    set(fpo_res_path_so "${HLS_FPO_DIR}/libIp_floating_point_v7_1_bitacc_cmodel.so" CACHE FILEPATH "" FORCE)
    message(STATUS fpo_res_path_so=${fpo_res_path_so})
  endif()
  if (EXISTS "${HLS_FPO_DIR}/libgmp.so.11")
    set(gmp_res_path_so "${HLS_FPO_DIR}/libgmp.so.11" CACHE FILEPATH "" FORCE)
    message(STATUS gmp_res_path_so=${gmp_res_path_so})
  endif()
  if (EXISTS "${HLS_FPO_DIR}/libmpfr.so.4")
    set(mpfr_res_path_so "${HLS_FPO_DIR}/libmpfr.so.4" CACHE FILEPATH "" FORCE)
    message(STATUS mpfr_res_path_so=${mpfr_res_path_so})
  endif()
endif()
#################################################

set(system_libs)
if ( LLVM_ENABLE_ZLIB AND HAVE_LIBZ )
  set(system_libs ${system_libs} ${ZLIB_LIBRARIES})
endif()
#if( MSVC OR MINGW )
if(WIN32 OR CYGWIN)
  if (DEFINED LINK_DIRS)
    set_property(TARGET sqlite3 PROPERTY IMPORTED_IMPLIB ${sqlite_res_path_lib})
  else()
    set_property(TARGET sqlite3 PROPERTY IMPORTED_LOCATION ${RDI_SRC_ROOT}/ext/sqlite/sqlite-3.45.1/rdi/lib/vs2017/win64.o/libsqlite3.45.1.dll)
    set_property(TARGET sqlite3 PROPERTY IMPORTED_IMPLIB ${RDI_SRC_ROOT}/ext/sqlite/sqlite-3.45.1/rdi/lib/vs2017/win64.o/libsqlite3.45.1.lib)
  endif()
  # fpo_v7_1
  set_property(TARGET fpo_v7_1 PROPERTY IMPORTED_LOCATION ${fpo_res_path_dll})
  set_property(TARGET fpo_v7_1 PROPERTY IMPORTED_IMPLIB ${fpo_res_path_lib})
  set_property(TARGET fpo_v7_1_gmp PROPERTY IMPORTED_LOCATION ${gmp_res_path_dll})
  set_property(TARGET fpo_v7_1_gmp PROPERTY IMPORTED_IMPLIB ${gmp_res_path_lib})
  set_property(TARGET fpo_v7_1_mpfr PROPERTY IMPORTED_LOCATION ${mpfr_res_path_dll})
  set_property(TARGET fpo_v7_1_mpfr PROPERTY IMPORTED_IMPLIB ${mpfr_res_path_lib})
  # libuuid required for FOLDERID_Profile usage in lib/Support/Windows/Path.inc.
  set(system_libs ${system_libs} psapi shell32 ole32 uuid)
#elseif( CMAKE_HOST_UNIX )
else()
  if (DEFINED LINK_DIRS)
    set_property(TARGET sqlite3 PROPERTY IMPORTED_NO_SONAME 1)
    set_target_properties(sqlite3 PROPERTIES IMPORTED_LOCATION ${sqlite_res_path_so})
  else() 
    if (DEFINED OPEN_SQLITE_LIB_FILE)
      set_property(TARGET sqlite3 PROPERTY IMPORTED_NO_SONAME 1)
      set_target_properties(sqlite3 PROPERTIES IMPORTED_LOCATION ${OPEN_SQLITE_LIB_FILE})
    else()
      set_property(TARGET sqlite3 PROPERTY IMPORTED_NO_SONAME 1)
      set_target_properties(sqlite3 PROPERTIES IMPORTED_LOCATION ${RDI_SRC_ROOT}/ext/sqlite/sqlite-3.45.1/rdi/lib/lnx64.o/libsqlite3.45.1.so)
    endif()
  endif()

  # fpo_v7_1
  set_property(TARGET fpo_v7_1 PROPERTY IMPORTED_NO_SONAME 1)
  set_target_properties(fpo_v7_1 PROPERTIES IMPORTED_LOCATION ${fpo_res_path_so})
  set_property(TARGET fpo_v7_1_gmp PROPERTY IMPORTED_NO_SONAME 1)
  set_target_properties(fpo_v7_1_gmp PROPERTIES IMPORTED_LOCATION ${gmp_res_path_so})
  set_property(TARGET fpo_v7_1_mpfr PROPERTY IMPORTED_NO_SONAME 1)
  set_target_properties(fpo_v7_1_mpfr PROPERTIES IMPORTED_LOCATION ${mpfr_res_path_so})

  if( HAVE_LIBRT )
    set(system_libs ${system_libs} rt)
  endif()
  if( HAVE_LIBDL )
    set(system_libs ${system_libs} ${CMAKE_DL_LIBS})
  endif()
  if( HAVE_BACKTRACE )
    set(system_libs ${system_libs} ${Backtrace_LIBRARIES})
  endif()
  if(LLVM_ENABLE_TERMINFO)
    if(HAVE_TERMINFO)
      set(system_libs ${system_libs} ${TERMINFO_LIBS})
    endif()
  endif()
  if( LLVM_ENABLE_THREADS AND HAVE_LIBATOMIC )
    set(system_libs ${system_libs} atomic)
  endif()
  set(system_libs ${system_libs} ${LLVM_PTHREAD_LIB})
  if( UNIX AND NOT (BEOS OR HAIKU) )
    set(system_libs ${system_libs} m)
  endif()
endif()

add_llvm_library(LLVMSupport
  AMDGPUMetadata.cpp
  APFloat.cpp
  APInt.cpp
  APSInt.cpp
  ARMBuildAttrs.cpp
  ARMAttributeParser.cpp
  ARMWinEH.cpp
  Allocator.cpp
  BinaryStreamError.cpp
  BinaryStreamReader.cpp
  BinaryStreamRef.cpp
  BinaryStreamWriter.cpp
  BlockFrequency.cpp
  BranchProbability.cpp
  CachePruning.cpp
  circular_raw_ostream.cpp
  Chrono.cpp
  COM.cpp
  CodeGenCoverage.cpp
  CommandLine.cpp
  Compression.cpp
  ConvertUTF.cpp
  ConvertUTFWrapper.cpp
  CrashRecoveryContext.cpp
  DataExtractor.cpp
  Debug.cpp
  DebugCounter.cpp
  DeltaAlgorithm.cpp
  DAGDeltaAlgorithm.cpp
  Error.cpp
  ErrorHandling.cpp
  FileUtilities.cpp
  FileOutputBuffer.cpp
  FoldingSet.cpp
  FormattedStream.cpp
  FormatVariadic.cpp
  GlobPattern.cpp
  GraphWriter.cpp
  Hashing.cpp
  IntEqClasses.cpp
  IntervalMap.cpp
  JamCRC.cpp
  JSON.cpp
  KnownBits.cpp
  LEB128.cpp
  LineIterator.cpp
  Locale.cpp
  LockFileManager.cpp
  LowLevelType.cpp
  ManagedStatic.cpp
  MathExtras.cpp
  MemoryBuffer.cpp
  MD5.cpp
  NativeFormatting.cpp
  Options.cpp
  Parallel.cpp
  PluginLoader.cpp
  PrettyStackTrace.cpp
  RandomNumberGenerator.cpp
  Regex.cpp
  ScaledNumber.cpp
  ScopedPrinter.cpp
  SHA1.cpp
  SmallPtrSet.cpp
  SmallVector.cpp
  SourceMgr.cpp
  SpecialCaseList.cpp
  Statistic.cpp
  StringExtras.cpp
  StringMap.cpp
  StringPool.cpp
  StringSaver.cpp
  StringRef.cpp
  SystemUtils.cpp
  TarWriter.cpp
  TargetParser.cpp
  ThreadPool.cpp
  Timer.cpp
  ToolOutputFile.cpp
  TrigramIndex.cpp
  Triple.cpp
  Twine.cpp
  Unicode.cpp
  XILINXSystemInfo.cpp
  YAMLParser.cpp
  YAMLTraits.cpp
  raw_os_ostream.cpp
  raw_ostream.cpp
  regcomp.c
  regerror.c
  regexec.c
  regfree.c
  regstrlcpy.c
  xxhash.cpp
  
  #Platform 
  XILINXFPGAPlatformBasic.cpp
  XILINXFPGACoreInstFactory.cpp
  XILINXFPGAFloatInstEval.cpp
  XilinxPlat/PortAndExprTreeNode.cpp
  XilinxPlat/SqliteSelector.cpp
  XilinxPlat/CoreInst.cpp
  XilinxPlat/CoreQuerier.cpp
  XilinxPlat/ChipInfo.cpp
  XilinxPlat/TargetPlatform.cpp

# System
  Atomic.cpp
  DynamicLibrary.cpp
  Errno.cpp
  Host.cpp
  Memory.cpp
  Mutex.cpp
  Path.cpp
  Process.cpp
  Program.cpp
  RWMutex.cpp
  Signals.cpp
  TargetRegistry.cpp
  ThreadLocal.cpp
  Threading.cpp
  Valgrind.cpp
  Watchdog.cpp

  ADDITIONAL_HEADER_DIRS
  Unix
  Windows
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/ADT
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Support
  ${Backtrace_INCLUDE_DIRS}
  LINK_LIBS ${system_libs} 
  )

######  only for debugging ##########
if (DEFINED INCLUDE_DIRS)
  message(STATUS gradle_INCLUDE_DIRS=${INCLUDE_DIRS})
  foreach(dir ${INCLUDE_DIRS})
    message(STATUS each_include_dir=${dir})
    if (EXISTS "${dir}/sqlite3.h") 
      set(sqlite3_header "{dir}/sqlite3.h" CACHE FILEPATH "" FORCE)
      message(STATUS sqlite3_header=${sqlite3_header})
      break()
    endif()
  endforeach()
else()
message(STATUS "Cannot find gradle_INCLUDE_DIRS.... ")
endif()
######  only for debugging ##########

if (DEFINED INCLUDE_DIRS)
  target_include_directories(LLVMSupport PRIVATE ${INCLUDE_DIRS})
else()
  if (DEFINED OPEN_SQLITE_INCLUDE_DIR)
    target_include_directories(LLVMSupport PRIVATE ${OPEN_SQLITE_INCLUDE_DIR})
  else()
    target_include_directories(LLVMSupport PRIVATE ${RDI_SRC_ROOT}/ext/sqlite/sqlite-3.45.1/rdi/include/)
  endif()
endif()

target_link_libraries(LLVMSupport PRIVATE sqlite3)

target_include_directories(LLVMSupport PRIVATE "${HLS_FPO_DIR}/")

target_link_libraries(LLVMSupport PRIVATE fpo_v7_1 fpo_v7_1_gmp fpo_v7_1_mpfr)

# Copy all the shared libraries to the lib folder on Linux (as RPATH will point there) or to the bin folder on Windows
if (WIN32 OR CYGWIN)
  set(LIB_OUTPUT_DIR "$<TARGET_FILE_DIR:LLVMSupport>")
  message(STATUS "LIB_OUTPUT_DIR=${LIB_OUTPUT_DIR}")
  add_custom_command(TARGET LLVMSupport POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${fpo_res_path_dll}" "${LIB_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${gmp_res_path_dll}" "${LIB_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${mpfr_res_path_dll}" "${LIB_OUTPUT_DIR}"
    COMMENT "Copying shared libraries to ${LIB_OUTPUT_DIR}"
    )
  install(FILES "${fpo_res_path_dll}" "${gmp_res_path_dll}" "${mpfr_res_path_dll}" DESTINATION bin)
else()
  get_target_property(RPATH LLVMSupport INSTALL_RPATH)
  if (NOT RPATH)
    set(RPATH "${CMAKE_INSTALL_RPATH}")
  endif()
  if (NOT RPATH)
    set(RPATH "\$ORIGIN/../lib")
  endif()
  string(REPLACE "\$ORIGIN" "$<TARGET_FILE_DIR:LLVMSupport>" LIB_OUTPUT_DIR "${RPATH}")
  message(STATUS "LIB_OUTPUT_DIR=${LIB_OUTPUT_DIR}")
  add_custom_command(TARGET LLVMSupport POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${fpo_res_path_so}" "${LIB_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${gmp_res_path_so}" "${LIB_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${mpfr_res_path_so}" "${LIB_OUTPUT_DIR}"
    COMMENT "Copying shared libraries to ${LIB_OUTPUT_DIR}"
    )
  install(FILES "${fpo_res_path_so}" "${gmp_res_path_so}" "${mpfr_res_path_so}" DESTINATION lib)
endif()


if (DEFINED RDI_BUILD_DIR)
  message(STATUS "RDI_BUILD_DIR=${RDI_BUILD_DIR}")
  link_directories(${RDI_BUILD_DIR}/ext)
elseif (DEFINED LINK_DIRS)
  link_directories("${LINK_DIRS}")
else()
  #message(FATAL_ERROR "Cannot find environment variable RDI_BUILD_DIR or LINK_DIRS. Please set it....")
endif()

link_directories("${HLS_FPO_DIR}")

set_property(TARGET LLVMSupport PROPERTY LLVM_SYSTEM_LIBS "${system_libs}")
add_definitions(-DXILINX_HLS_FE_STANDALONE)
