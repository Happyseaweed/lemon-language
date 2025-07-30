#include "llvm/ADT/APFloat.h" 		// Definitely need to move these somewhere else, IWYU
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"

// ============================================================================

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <string>

// ============================================================================

using namespace llvm;
// ============================================================================
// Tokens 
enum Token {
	tok_eof = -1,

	// commands
	tok_def = -2,
	tok_extern = -3,

	// Primary
	tok_identifier = -4,
	tok_number = -5,

    // Control
    tok_if = -6,
    tok_then = -7,
    tok_else = -8,

    // Loops
    tok_for = -9,
    tok_in = -10,

    // User-defined ops
    tok_binary = -11,
    tok_unary = -12,

	// User-defined variables
    tok_var = -13,

	// More lemon features 
	tok_lbrace = -14,
	tok_rbrace = -15,
	tok_semi = -16,
	
	tok_equal = -17,
	tok_return = -19,
	tok_while = -20
};

// ============================================================================
// lexer.h
#pragma once

extern std::string IdentifierStr; 	// Filled in if tok_identifier
extern double NumVal; 				// Filled in if tok_number
extern int CurTok;					// Current token

// ============================================================================
int gettok();

int getNextToken();