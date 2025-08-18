// Lemon Semantic Analyzer
#pragma once

#include "./AST.h"

// Semantic Analyzer for Lemon.
class SemanticAnalyzer {
public:
    void visit(const std::unique_ptr<LemonAST> &lemonProgram);
    void visit(const std::unique_ptr<StmtAST> &statement);
    void visit(const std::unique_ptr<ExprAST> &expression);

    void error(const std::string errorMessage) const;
};