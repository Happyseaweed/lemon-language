# CMAKE Lists - July 2025
# Siwei Du

cmake_minimum_required(VERSION 3.15)
project(lemon)

# Using C++17 because why not. 
# Is this a bad thing to not have a good reason? 
# 17 is industry standard after googling.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)

# LLVM setup
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# All the include dirs, these variables should be in the LLVM package.
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# List of source files
set(SOURCES
    src/lemon.cc
    src/parser.cc
    src/lexer.cc
    src/ast.cc
)

add_executable(lemon ${SOURCES})

# Pick LLVM components you use
# I built mine on M1 MacOS, might have to change it if you 
# are on another architecture.

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
