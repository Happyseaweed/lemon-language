// ============================================================================
// AST Declarations 
// ============================================================================
#pragma once

#include "llvm/IR/Value.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/Reassociate.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Transforms/Scalar/ADCE.h"
#include "llvm/Transforms/Scalar/DeadStoreElimination.h"
#include "llvm/Transforms/Scalar/DCE.h"
#include <llvm/Support/TargetSelect.h>
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/DerivedTypes.h"

#include "./LemonJIT.h"
#include "./Types.h"

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <stack>

using namespace llvm;
using namespace llvm::orc;


// EXPRESSION
class ExprAST {
public:
    mutable LemonType::Type type;
    virtual ~ExprAST() = default;
    virtual Value *codegen(const std::string scope) = 0;
    virtual void showAST() = 0;
};

// STATEMENT
class StmtAST {
public:
    virtual ~StmtAST() = default;
    virtual Value *codegen(const std::string scope) = 0;
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

    Value *codegen(const std::string scope = "_global");
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
    
    Value *codegen(const std::string scope) override;
    void showAST() override;

    // Helpers
    const char getOp() const { return op; }
};

class NumberExprAST : public ExprAST {
    double val;
public:
    NumberExprAST(double val)
        : val(val) {}
    
    Value *codegen(const std::string scope) override;
    void showAST() override;

    // Helpers
    const double getVal() const { return val; }
};

class VariableExprAST : public ExprAST {
    std::string varName;
public:
    VariableExprAST(const std::string &varName) 
        : varName(varName) {}

    Value *codegen(const std::string scope) override;
    void showAST() override;

    // Helpers
    const std::string getVarName() const { return varName; }
};

class TensorExprAST : public ExprAST {
    std::vector<size_t> shape;
    std::vector<std::unique_ptr<ExprAST>> values;

public:
    TensorExprAST(std::vector<size_t> shape, std::vector<std::unique_ptr<ExprAST>> values)
        : shape(std::move(shape)), values(std::move(values)) {}

    Value *codegen(const std::string scope) override;
    void showAST() override;

    const std::vector<size_t> getShape() const { return shape; }
    const size_t getSize() const { return values.size(); }
    const std::vector<std::unique_ptr<ExprAST>> &getValues() { return values; }
};

class SubscriptExprAST : public ExprAST {
    std::string varName;
    std::vector<std::unique_ptr<ExprAST>> subscripts;
public:
    SubscriptExprAST(std::string varName, 
                     std::vector<std::unique_ptr<ExprAST>> subscripts)
        : varName(varName), subscripts(std::move(subscripts)) {}
    
    Value *codegen(const std::string scope) override;
    void showAST() override;
};

class CallExprAST : public ExprAST {
    std::string callee; 
    std::vector<std::unique_ptr<ExprAST>> args;
public:
    CallExprAST(std::string callee, std::vector<std::unique_ptr<ExprAST>> args)
        : callee(callee), args(std::move(args)) {}
    
    Value *codegen(const std::string scope) override;
    void showAST() override;
};

// STATEMENT
// Function signatures are always IDs and IDs only.
//      => Calls can contain EXPRs for args.

class PrototypeAST {
    std::string name;
    std::vector<std::string> args;

public:
    PrototypeAST(const std::string name, std::vector<std::string> args)
        : name(name), args(std::move(args)) {}

    Function *codegen(const std::string scope = "_global");
    void showAST();

    const std::string getName() const { return name; }
};

class VariableDeclStmt : public StmtAST {
    std::string varName;
    std::unique_ptr<ExprAST> defBody;
public:
    VariableDeclStmt(std::string varName, std::unique_ptr<ExprAST> defBody) 
        : varName(varName), defBody(std::move(defBody)) {}

    Value *codegen(const std::string scope) override;
    Value *codegen_global();
    void showAST() override;
};

// Same as var decl, can just replace?
class AssignmentStmt : public StmtAST {
    std::string varName;
    std::unique_ptr<ExprAST> defBody;
public:
    AssignmentStmt(std::string varName, std::unique_ptr<ExprAST> defBody) 
        : varName(varName), defBody(std::move(defBody)) {}

    Value *codegen(const std::string scope) override;
    void showAST() override;
};

class ReturnStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> retBody;
public:
    ReturnStmtAST(std::unique_ptr<ExprAST> retBody)
        : retBody(std::move(retBody)) {}

    Value *codegen(const std::string scope) override;
    void showAST() override;
};

class FunctionAST : public StmtAST {
    std::unique_ptr<PrototypeAST> proto;
    std::vector<std::unique_ptr<StmtAST>> functionBody;
public:
    FunctionAST(std::unique_ptr<PrototypeAST> proto, 
                std::vector<std::unique_ptr<StmtAST>> functionBody)
        : proto(std::move(proto)), functionBody(std::move(functionBody)) {}
    
    Value *codegen(const std::string scope = "_global") override; // Returns Function *
    void showAST() override;
};

class ExternAST : public StmtAST {
    std::unique_ptr<PrototypeAST> proto;
public:
    ExternAST(std::unique_ptr<PrototypeAST> proto)
        : proto(std::move(proto)) {}
    
    Value *codegen(const std::string scope) override;
    void showAST() override;
};

class ExpressionStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> expr;
public:
    ExpressionStmtAST(std::unique_ptr<ExprAST> expr)
        : expr(std::move(expr)) {}

    Value *codegen(const std::string scope) override;
    void showAST() override;
};

class IfStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> cond;
    std::vector<std::unique_ptr<StmtAST>> thenBody;
    std::vector<std::unique_ptr<StmtAST>> elseBody;

public:
    IfStmtAST(std::unique_ptr<ExprAST> cond,
              std::vector<std::unique_ptr<StmtAST>> thenBody,
              std::vector<std::unique_ptr<StmtAST>> elseBody)
        : cond(std::move(cond)), thenBody(std::move(thenBody)), 
          elseBody(std::move(elseBody)) {}
    
    Value *codegen(const std::string scope) override;
    void showAST() override;
};

class ForStmtAST : public StmtAST {
    std::string iterator;
    std::unique_ptr<ExprAST> start, end, step;
    std::vector<std::unique_ptr<StmtAST>> forBody;
public:
    ForStmtAST(const std::string &iterator, 
               std::unique_ptr<ExprAST> start,
               std::unique_ptr<ExprAST> end,
               std::unique_ptr<ExprAST> step,
               std::vector<std::unique_ptr<StmtAST>> forBody)
        : iterator(iterator), start(std::move(start)), end(std::move(end)),
          step(std::move(step)), forBody(std::move(forBody)) {}
    
    Value *codegen(const std::string scope) override;
    void showAST() override;
};

// Core Variables and Helper functions

extern std::unique_ptr<LLVMContext> TheContext;
extern std::unique_ptr<IRBuilder<>> Builder;
extern std::unique_ptr<IRBuilder<>> GlobalVariableBuilder;
extern std::unique_ptr<IRBuilder<>> MainBuilder;
extern std::unique_ptr<IRBuilder<>> FunctionBuilder;

extern std::unique_ptr<Module> TheModule;
// extern std::map<std::string, std::map<std::string, AllocaInst*>> SymbolTable; // Stores all variables
// extern std::stack<std::string> ScopeStack;
extern std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

extern int LoopScopeCounter;
extern std::string generateLoopScope();

extern AllocaInst *CreateEntryBlockAlloca(Function *TheFunction, StringRef varName);
extern AllocaInst *CreateEntryBlockAllocaTensor(Function *TheFunction, StringRef varName, size_t numElements);
extern Function *getFunction(std::string name, std::string scope = "_global");

extern std::unique_ptr<FunctionPassManager> TheFPM;
extern std::unique_ptr<LoopAnalysisManager> TheLAM;
extern std::unique_ptr<FunctionAnalysisManager> TheFAM;
extern std::unique_ptr<CGSCCAnalysisManager> TheCGAM;
extern std::unique_ptr<ModuleAnalysisManager> TheMAM;
extern std::unique_ptr<PassInstrumentationCallbacks> ThePIC;
extern std::unique_ptr<StandardInstrumentations> TheSI;

// JIT
extern std::unique_ptr<LemonJIT> TheJIT;
