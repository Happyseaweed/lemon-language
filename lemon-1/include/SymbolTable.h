// Symbol table storing information about variables, functions, etc.
#pragma once

#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"
#include "./Types.h"

#include <string>

typedef struct Symbol {
    llvm::AllocaInst* alloca = nullptr;
    LemonType::Type type;
} Symbol;

// [scope][var name, symbol]
extern std::vector<std::pair<std::string, std::unordered_map<std::string, Symbol>>> SymbolTable;

Symbol* findSymbol(const std::string& scope, const std::string& variableName) {
    Symbol* ret = nullptr; 
    int n = SymbolTable.size()-1;
    
    for (int i = n; i >= 0; --i) {
        if (SymbolTable[i].first == scope){
            if (SymbolTable[i].second[variableName].alloca == nullptr)
                continue;

            ret = &SymbolTable[i].second[variableName];
            break;
        }
    }
    return ret;
}

void addSymbol(const std::string& variableName, llvm::AllocaInst* alloca, LemonType::TypeKind type) {
    SymbolTable.back().second[variableName].alloca = alloca;
    SymbolTable.back().second[variableName].type.kind = type;
}

void enterScope(const std::string& scope) {
    SymbolTable.push_back(make_pair(scope, std::unordered_map<std::string, Symbol>()));
}

void leaveScope() {
    SymbolTable.pop_back();
}