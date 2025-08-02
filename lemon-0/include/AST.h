#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/IR/Verifier.h"
// ============================================================================

#include "../include/LemonJIT.h"

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
using namespace llvm::orc;

// ============================================================================
#pragma once
extern std::unique_ptr<LLVMContext> TheContext;     // Contains core LLVM datastructures
extern std::unique_ptr<IRBuilder<>> Builder;        // Used for generating IR instructions
extern std::unique_ptr<Module> TheModule;           // Conains all generated IR
extern std::map<std::string, AllocaInst*> NamedValues;  // Symbol table for the code and its generated IR

extern std::unique_ptr<FunctionPassManager> TheFPM;
extern std::unique_ptr<LoopAnalysisManager> TheLAM;
extern std::unique_ptr<FunctionAnalysisManager> TheFAM;
extern std::unique_ptr<CGSCCAnalysisManager> TheCGAM;
extern std::unique_ptr<ModuleAnalysisManager> TheMAM;
extern std::unique_ptr<PassInstrumentationCallbacks> ThePIC;
extern std::unique_ptr<StandardInstrumentations> TheSI;

extern std::unique_ptr<LemonJIT> TheJIT;
extern std::map<char, int> BinopPrecedence;


// ============================================================================
//                              Helper Functions
//                            (And forward decls.)
// ============================================================================

Value *LogErrorV(const char *Str);

static AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
                                          StringRef VarName) {
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                     TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(Type::getDoubleTy(*TheContext), nullptr, VarName);
}

// ============================================================================
class ExprAST  {
public:
    virtual ~ExprAST() = default;
    virtual Value *codegen() = 0;
};

// ============================================================================
class StmtAST {
public:
    virtual ~StmtAST() = default;
    virtual void codegen() = 0;
};

// ============================================================================
class NumberExprAST : public ExprAST {
    double Val;
public:
    NumberExprAST(double val) : Val(val) {}

    Value *codegen() override;
};

// ============================================================================
class VariableExprAST : public ExprAST {
    std::string Name;
public:
    VariableExprAST(const std::string &Name): Name(Name) {}
    const std::string &getName() const { return Name; }

    Value *codegen() override;
};


// ============================================================================
class IfExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Cond, Then, Else;
public:
    IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then, 
              std::unique_ptr<ExprAST> Else)
        : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
    
    Value *codegen() override;
};


// ============================================================================
class ForExprAST : public ExprAST {
    std::string VarName;
    std::unique_ptr<ExprAST> Start, End, Step, Body;

public:
    ForExprAST(const std::string &VarName, std::unique_ptr<ExprAST> Start,
               std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
               std::unique_ptr<ExprAST> Body)
        : VarName(VarName), Start(std::move(Start)), End(std::move(End)),
          Step(std::move(Step)), Body(std::move(Body)) {}
    
    Value *codegen() override;
};

// ============================================================================
class UnaryExprAST : public ExprAST {
    char Opcode;
    std::unique_ptr<ExprAST> Operand;

public:
    UnaryExprAST(char Opcode, std::unique_ptr<ExprAST> Operand)
        : Opcode(Opcode), Operand(std::move(Operand)) {}

    Value *codegen() override;
};

// ============================================================================
class VarExprAST : public ExprAST {
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST> > > VarNames;
    std::unique_ptr<ExprAST> Body;

public:
    VarExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST> > > VarNames,
               std::unique_ptr<ExprAST> Body)
        : VarNames(std::move(VarNames)), Body(std::move(Body)) {}
    
    Value *codegen() override;

};

// ============================================================================
class PrototypeAST {
    std::string Name;
    std::vector<std::string> Args;
    bool IsOperator;
    unsigned Precedence; // flag for binary op 

public:
    PrototypeAST(const std::string &Name, std::vector<std::string> Args, 
                 bool IsOperator = false, unsigned Prec = 0)
        : Name(Name), Args(std::move(Args)), IsOperator(IsOperator), Precedence(Prec) {}
    
    const std::string &getName() const { return Name; }

    bool isUnaryOp() const { return IsOperator && Args.size() == 1; }
    bool isBinaryOp() const { return IsOperator && Args.size() == 2; }

    char getOperatorName() const {
        assert(isUnaryOp() || isBinaryOp());
        return Name[Name.size() - 1];
    }

    unsigned getBinaryPrecedence() const { return Precedence; }
    
    Function *codegen();
};

// ============================================================================
class BinaryExprAST : public ExprAST {
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(char Op, 
                  std::unique_ptr<ExprAST> LHS, 
                  std::unique_ptr<ExprAST> RHS) 
        : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

    Value *codegen() override;
};

// ============================================================================
class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST> > Args;

public:
    CallExprAST(const std::string &Callee, 
                std::vector<std::unique_ptr<ExprAST> > Args)
        : Callee(Callee), Args(std::move(Args)) {}
    Value *codegen() override;
};

// ============================================================================
class FunctionAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> ExprBody = nullptr;
    std::unique_ptr<StmtAST> StmtBody = nullptr;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto, 
                std::unique_ptr<ExprAST> Body)
        : Proto(std::move(Proto)), ExprBody(std::move(Body)) {}

    FunctionAST(std::unique_ptr<PrototypeAST> Proto, 
                std::unique_ptr<StmtAST> Body)
        : Proto(std::move(Proto)), StmtBody(std::move(Body)) {}

    Function *codegen();
};


// ============================================================================
//                              Statement AST    
// ============================================================================

// ============================================================================
class VarDeclStmtAST : public StmtAST {
    std::string Name;
    std::unique_ptr<ExprAST> Init;

public:
    VarDeclStmtAST(std::string Name, std::unique_ptr<ExprAST> Init)
        : Name(Name), Init(std::move(Init)) {}

    void codegen() override;
};

class AssignmentStmtAST : public StmtAST {
    std::string Name;
    std::unique_ptr<ExprAST> Expr;
public:
    AssignmentStmtAST(std::string Name, std::unique_ptr<ExprAST> Expr)
        : Name(Name), Expr(std::move(Expr)) {}

    void codegen() override;
};

// ============================================================================
//                            Helper getFunction()
// ============================================================================
Function *getFunction(std::string Name);

extern std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;