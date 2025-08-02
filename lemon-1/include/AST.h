// ============================================================================
// AST Declarations 
// ============================================================================
#include "llvm/IR/Value.h"

#include <string>
#include <vector>
#include <memory>

using namespace llvm;

#pragma once


// EXPRESSION
class ExprAST {
public:
    virtual ~ExprAST() = default;
    virtual Value *codegen() = 0;
    virtual void showAST() = 0;
};

// STATEMENT
class StmtAST {
public:
    virtual ~StmtAST() = default;
    virtual Value *codegen() = 0;
    virtual void showAST() = 0;
};

// MAIN LEMON
class LemonAST {
    std::vector<std::unique_ptr<StmtAST>> statements;
    uint64_t optimizations;
public:
    LemonAST(std::vector<std::unique_ptr<StmtAST>> statements, 
             uint64_t optimizations)
        : statements(std::move(statements)), optimizations(optimizations) {}

    Value *codegen();
    void showAST();
};

// Sub Trees
class BinaryExprAST : public ExprAST {
    int op;
    std::unique_ptr<ExprAST> LHS, RHS;
public:
    BinaryExprAST(int op, 
                  std::unique_ptr<ExprAST> LHS, 
                  std::unique_ptr<ExprAST> RHS)
        : op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
    
    Value *codegen() override;
    void showAST() override;

    // Helpers
    const char getOp() const { return op; }
};

class NumberExprAST : public ExprAST {
    double val;
public:
    NumberExprAST(double val)
        : val(val) {}
    
    Value *codegen() override;
    void showAST() override;

    // Helpers
    const double getVal() const { return val; }
};

class VariableExprAST : public ExprAST {
    std::string varName;
public:
    VariableExprAST(const std::string &varName) 
        : varName(varName) {}

    Value *codegen() override;
    void showAST() override;

    // Helpers
    const std::string getVarName() const { return varName; }
};

class CallExprAST : public ExprAST {
    std::string callee; 
    std::vector<std::unique_ptr<ExprAST>> args;
public:
    CallExprAST(std::string callee, std::vector<std::unique_ptr<ExprAST>> args)
        : callee(callee), args(std::move(args)) {}
    
    Value *codegen() override;
    void showAST() override;
};

// STATEMENT

class PrototypeAST {
    std::string name;
    std::vector<std::string> args;

public:
    PrototypeAST(const std::string name, std::vector<std::string> args)
        : name(name), args(std::move(args)) {}

    Function *codegen();
    void showAST();
};

class VariableDeclStmt : public StmtAST {
    std::string varName;
    std::unique_ptr<ExprAST> defBody;
public:
    VariableDeclStmt(std::string varName, std::unique_ptr<ExprAST> defBody) 
        : varName(varName), defBody(std::move(defBody)) {}

    Value *codegen() override;
    void showAST() override;
};

// Same as var decl, can just replace?
class AssignmentStmt : public StmtAST {
    std::string varName;
    std::unique_ptr<ExprAST> defBody;
public:
    AssignmentStmt(std::string varName, std::unique_ptr<ExprAST> defBody) 
        : varName(varName), defBody(std::move(defBody)) {}

    Value *codegen() override;
    void showAST() override;
};

class ReturnStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> retBody;
public:
    ReturnStmtAST(std::unique_ptr<ExprAST> retBody)
        : retBody(std::move(retBody)) {}

    Value *codegen() override;
    void showAST() override;
};

class FunctionAST {
    std::unique_ptr<PrototypeAST> proto;
    std::vector<std::unique_ptr<StmtAST>> functionBody;
public:
    FunctionAST(std::unique_ptr<PrototypeAST> proto, 
                std::vector<std::unique_ptr<StmtAST>> functionBody)
        : proto(std::move(proto)), functionBody(std::move(functionBody)) {}
    
    Function *codegen();
    void showAST();
};

