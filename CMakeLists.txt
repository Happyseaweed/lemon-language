cmake_minimum_required(VERSION 3.15)
project(lemon)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)

# LLVM setup
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# List of your source files
set(SOURCES
    src/lemon.cc
    src/parser.cc
    src/lexer.cc
    src/ast.cc
)

add_executable(lemon ${SOURCES})

# Pick LLVM components you use
llvm_map_components_to_libnames(LLVM_LIBS
  core
  native
  nativecodegen
  orcjit
  executionengine
  jitlink
  passes
  transformutils
  analysis
  asmparser
  asmprinter
  target
  irreader
  support
  mc
  mcjit
  mcparser
  AArch64
  AArch64AsmParser
  AArch64CodeGen
  AArch64Desc
  AArch64Disassembler
  AArch64Info
  AArch64Utils
)

target_link_libraries(lemon ${LLVM_LIBS})
