#include "../include/SymbolTable.h"

using namespace Lemon;

std::vector<std::pair<std::string, std::unordered_map<std::string, Symbol>>> Lemon::SymbolTable;

Symbol* Lemon::findSymbol(const std::string& scope, const std::string& variableName) {
    Symbol* ret = nullptr; 
    int n = SymbolTable.size()-1;
    printf("Searching for symbol: (%s)\n", variableName.c_str());

    for (int i = n; i >= 0; --i) {
        if (SymbolTable[i].second[variableName].alloca == nullptr)
            continue;
        
        printf("Found symbol (%s) in scope (%s).\n", variableName.c_str(), scope.c_str());
        ret = &SymbolTable[i].second[variableName];
        break;
    }
    return ret;
}

void Lemon::addSymbol(const std::string& variableName, llvm::AllocaInst* alloca, LemonType::TypeKind type) {
    printf("addSymbol local variant called.\n");
    SymbolTable.back().second[variableName].alloca = alloca;
    SymbolTable.back().second[variableName].type.kind = type;
    SymbolTable.back().second[variableName].isGlobal = false;
}

void Lemon::addSymbol(const std::string& variableName, llvm::GlobalVariable* alloca, LemonType::TypeKind type) {
    printf("addSymbol global variant called.\n");
    // (for assignment below) If you don't import GlobalVariable.h this would give red squiggle.
    // First item in SymbolTable is always global symbol table.
    SymbolTable[0].second[variableName].alloca = alloca;        
    SymbolTable[0].second[variableName].type.kind = type;
    SymbolTable[0].second[variableName].isGlobal = true;
}

void Lemon::enterScope(const std::string& scope) {
    SymbolTable.push_back(make_pair(scope, std::unordered_map<std::string, Symbol>()));
}

void Lemon::leaveScope() {
    SymbolTable.pop_back();
}