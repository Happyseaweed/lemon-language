// ============================================================================
// Parser for Lemon-1
// ============================================================================
#include "llvm/IR/Value.h"
#include "../include/AST.h"

#include <memory>
#include <map>

using namespace llvm;

#pragma once
extern std::map<int, int> operatorPrecedence;

// Helper Functions
int getPrecedence(int tok);


// Error Functions
std::unique_ptr<ExprAST> LogError(const char *Str);

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);

std::unique_ptr<StmtAST> LogErrorS(const char *Str);

Value *LogErrorV(const char *Str);


// Parsing Functions
std::unique_ptr<LemonAST> Parse();

std::vector<std::unique_ptr<StmtAST>> ParseStatementList();

std::unique_ptr<StmtAST> ParseStatement();

std::unique_ptr<PrototypeAST> ParsePrototype();

std::unique_ptr<FunctionAST> ParseFunction();

std::vector<std::unique_ptr<ExprAST>> ParseArgList();

std::unique_ptr<StmtAST> ParseVariableDecl();

std::unique_ptr<StmtAST> ParseVariableAssign();

std::unique_ptr<StmtAST> ParseReturn();

std::unique_ptr<ExprAST> ParseExpression();

std::unique_ptr<ExprAST> ParseFactor();

std::unique_ptr<ExprAST> ParseBinOpRHS(int precedence, std::unique_ptr<ExprAST> LHS);

std::unique_ptr<ExprAST> ParseNumberExpr();

std::unique_ptr<ExprAST> ParseIdentifierExpr();