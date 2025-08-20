// Symbol table storing information about variables, functions, etc.
#pragma once

#include "llvm/IR/Value.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "./Types.h"

#include <string>

namespace Lemon {

typedef struct Symbol {
    llvm::Value* alloca = nullptr;
    // llvm::AllocaInst* alloca = nullptr;
    LemonType::Type type;
    bool isGlobal;
} Symbol;

// [scope][var name, symbol]
extern std::vector<std::pair<std::string, std::unordered_map<std::string, Symbol>>> SymbolTable;

Symbol* findSymbol(const std::string& scope, const std::string& variableName);

void addSymbol(const std::string& variableName, llvm::AllocaInst* alloca, LemonType::TypeKind type);

void addSymbol(const std::string& variableName, llvm::GlobalVariable* alloca, LemonType::TypeKind type);

void enterScope(const std::string& scope);

void leaveScope();

}