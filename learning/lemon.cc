#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;

// LLVM vars
static std::unique_ptr<LLVMContext> TheContext;     // Contains core LLVM datastructures
static std::unique_ptr<IRBuilder<>> Builder;        // Used for generating IR instructions
static std::unique_ptr<Module> TheModule;           // Conains all generated IR
static std::map<std::string, Value *> NamedValues;  // Symbol table for the code and its generated IR

enum Token {
	tok_eof = -1,

	// commands
	tok_def = -2,
	tok_extern = -3,

	// Primary
	tok_identifier = -4,
	tok_number = -5
};

static std::string IdentifierStr; 	// Filled in if tok_identifier
static double NumVal; 				// Filled in if tok_number


static int gettok() {
	static int LastChar = ' ';
		
	// Skipping whitespaces
	while (isspace(LastChar)) LastChar = getchar();

	if (isalpha(LastChar)) {
		IdentifierStr = LastChar;
		while (isalnum(LastChar = getchar()))
			IdentifierStr += LastChar;
		
		if (IdentifierStr == "def")
			return tok_def;
		if (IdentifierStr == "extern")
			return tok_extern;


		return tok_identifier;
	}

	if (isdigit(LastChar) || LastChar == '.') {
		std::string NumStr;

		do {
			NumStr += LastChar;
			LastChar = getchar();
		} while (isdigit(LastChar) || LastChar == '.');

		NumVal = strtod(NumStr.c_str(), 0);
		return tok_number;
	}

	if (LastChar == '#') {
		do 
			LastChar = getchar();
        while(LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF)
            return gettok();
	}

	if (LastChar == EOF) 
		return tok_eof;

	int ThisChar = LastChar;
	LastChar = getchar();
	return ThisChar;
}

Value *LogErrorV(const char *Str);


class ExprAST  {
public:
    virtual ~ExprAST() = default;
    virtual Value *codegen() = 0;
};

class NumberExprAST : public ExprAST {
    double Val;
public:
    NumberExprAST(double val) : Val(val) {}
    Value *codegen() override {
        return ConstantFP::get(*TheContext, APFloat(Val));
    }
};

class VariableExprAST : public ExprAST {
    std::string Name;
public:
    VariableExprAST(const std::string &Name): Name(Name) {}
    Value *codegen() override {
        Value *V = NamedValues[Name];

        if (!V) {
            LogErrorV("Unknown variable name");
        }
        return V;
    }
};

class BinaryExprAST : public ExprAST {
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(char Op, 
                  std::unique_ptr<ExprAST> LHS, 
                  std::unique_ptr<ExprAST> RHS) 
        : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
    Value *codegen() override {
        Value *L = LHS->codegen();
        Value *R = RHS->codegen();

        if (!L || !R)
            return nullptr;

        switch (Op) {
        case '+':
            return Builder->CreateFAdd(L, R, "addtmp");
        case '-':
            return Builder->CreateFSub(L, R, "subtmp");
        case '*':
            return Builder->CreateFMul(L, R, "multmp");
        case '<':
            L = Builder->CreateFCmpULT(L, R, "cmptmp");
            // Convert bool 0/1 to double 0.0 or 1.0
            return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext),
                                        "booltmp");
        default:
            return LogErrorV("invalid binary operator");
        }
    }
};

class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST> > Args;

public:
    CallExprAST(const std::string &Callee, 
                std::vector<std::unique_ptr<ExprAST> > Args)
        : Callee(Callee), Args(std::move(Args)) {}
    Value *codegen() override {
        // Look up the name in the global module table.
        Function *CalleeF = TheModule->getFunction(Callee);

        if (!CalleeF)
            return LogErrorV("Unknown function referenced");

        // If argument mismatch error.
        if (CalleeF->arg_size() != Args.size())
            return LogErrorV("Incorrect # arguments passed");

        std::vector<Value *> ArgsV;
        // Generates IR for all arguments first.
        for (unsigned i = 0, e = Args.size(); i != e; ++i) {
            ArgsV.push_back(Args[i]->codegen());
            if (!ArgsV.back())
            return nullptr;
        }

        return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
    }
};

class PrototypeAST {
    std::string Name;
    std::vector<std::string> Args;

public:
    PrototypeAST(const std::string &Name, std::vector<std::string> Args)
        : Name(Name), Args(std::move(Args)) {}
    
    const std::string &getName() const { return Name; }
    Function *codegen() {
        // Make the function type:  double(double,double) etc.
        std::vector<Type*> Doubles(Args.size(),
                                    Type::getDoubleTy(*TheContext));
        FunctionType *FT =
            FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false);

        Function *F =
            Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

        // Set names for all arguments.
        unsigned Idx = 0;
        for (auto &Arg : F->args())
        Arg.setName(Args[Idx++]);

        return F;
    }
};

class FunctionAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;
public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto, 
                std::unique_ptr<ExprAST> Body)
        : Proto(std::move(Proto)), Body(std::move(Body)) {}

    Function *codegen() {
        // First, check for an existing function from a previous 'extern' declaration.
        Function *TheFunction = TheModule->getFunction(Proto->getName());

        if (!TheFunction)
            TheFunction = Proto->codegen();

        if (!TheFunction)
            return nullptr;

        if (!TheFunction->empty())
            return (Function*)LogErrorV("Function cannot be redefined.");

        // Create a new basic block to start insertion into.`
        BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
        Builder->SetInsertPoint(BB);

        // Record the function arguments in the NamedValues map.
        NamedValues.clear();
        for (auto &Arg : TheFunction->args())
            NamedValues[std::string(Arg.getName())] = &Arg;
        
        if (Value *RetVal = Body->codegen()) {
            // Finish off the function.
            Builder->CreateRet(RetVal);

            // Validate the generated code, checking for consistency.
            verifyFunction(*TheFunction);

            return TheFunction;
        }
        // Error reading body, remove function.
        TheFunction->eraseFromParent();
        return nullptr;
    }
};

// ============================================================================

// 1 look-ahead buffer
static int CurTok;
static int getNextToken() {
    return CurTok = gettok();
}


std::unique_ptr<ExprAST> LogError(const char *Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
    LogError(Str);
    return nullptr;
}

Value *LogErrorV(const char *Str) {
  LogError(Str);
  return nullptr;
}

// Basic Expression Parsing
static std::unique_ptr<ExprAST> ParseExpression();

static std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto Result = std::make_unique<NumberExprAST>(NumVal);
    getNextToken(); // Consume
    return std::move(Result);
}


static std::unique_ptr<ExprAST> ParseParenExpr() {
    getNextToken();
    auto V = ParseExpression(); // Parse inside brackets
    if (!V) 
        return nullptr;

    if (CurTok != ')') 
        return LogError("Expected ')'");

    getNextToken(); 
    return V;
}

static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    std::string IdName = IdentifierStr;

    getNextToken(); // Consume

    if (CurTok != '(')
        return std::make_unique<VariableExprAST>(IdName);

    getNextToken();
    std::vector<std::unique_ptr<ExprAST> > Args;

    if (CurTok != ')') {
        while(true) {
            if (auto Arg = ParseExpression())
                Args.push_back(std::move(Arg));
            else 
                return nullptr;

            if (CurTok == ')')  // end of params
                break; 
            
            if (CurTok != ',') 
                return LogError("Expected ')' or ',' in argument list");

            getNextToken();
        }
    }

    getNextToken(); // consume ')'

    return std::make_unique<CallExprAST>(IdName, std::move(Args));
}


static std::unique_ptr<ExprAST> ParsePrimary() {
    switch (CurTok) {
        default:
            return LogError("Unknown token when expecting an expression");
        case tok_identifier:
            return ParseIdentifierExpr();
        case tok_number:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
    }
}

// Binary OP parsing ==========================================================
static std::map<char, int> BinopPrecedence;

static int GetTokPrecedence() {
    if (!isascii(CurTok)) 
        return -1;

    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0) 
        return -1;

    return TokPrec;
}

static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS) {
    while (true) {
        int TokPrec = GetTokPrecedence();

        // Interesting
        if (TokPrec < ExprPrec)
            return LHS;

        int BinOp = CurTok;
        getNextToken();

        auto RHS = ParsePrimary();
        if (!RHS)
            return nullptr;

        int NextPrec = GetTokPrecedence();
        if (TokPrec < NextPrec) {
            RHS = ParseBinOpRHS(TokPrec+1, std::move(RHS));
            if (!RHS) 
                return nullptr;
        }

        LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
    }
}

static std::unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParsePrimary();
    if (!LHS) 
        return nullptr;

    return ParseBinOpRHS(0, std::move(LHS));
}

static std::unique_ptr<PrototypeAST> ParsePrototype() {
    if (CurTok != tok_identifier)
        return LogErrorP("Expected function name in prototype.");

    std::string FnName = IdentifierStr;
    getNextToken();

    if (CurTok != '(')
        return LogErrorP("Expected '(' in prototype");

    std::vector<std::string> ArgNames;
    while (getNextToken() == tok_identifier) {
        ArgNames.push_back(IdentifierStr);
    }
    if (CurTok != ')')
        return LogErrorP("Expected ')' in prototype");

    getNextToken();

    return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

static std::unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken();
    auto Proto = ParsePrototype();

    if (!Proto) return nullptr;

    if (auto E = ParseExpression())
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));

    return nullptr;
}

static std::unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken();
    return ParsePrototype(); // Function signature
}

// Allow for arbitrary top level expressions and realtime evaluation.
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if (auto E = ParseExpression()) {
        auto Proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}

static void HandleDefinition() {
  if (ParseDefinition()) {
    fprintf(stderr, "Parsed a function definition.\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleExtern() {
  if (ParseExtern()) {
    fprintf(stderr, "Parsed an extern\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (ParseTopLevelExpr()) {
    fprintf(stderr, "Parsed a top-level expr\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}


static void MainLoop() {
    while(true) {
        fprintf(stderr, "ready> ");
        switch (CurTok)  {
            case tok_eof:
                return;
            case ';':
                getNextToken();
                break;
            case tok_def:
                HandleDefinition();
                break;
            case tok_extern:
                HandleExtern();
                break;
            default:
                HandleTopLevelExpression();
                break;
        }
    }
}

int main() {
    // Smaller number = lower precedence.
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 30;
    BinopPrecedence['*'] = 40;

    fprintf(stderr, "ready> ");
    getNextToken();

    MainLoop();

    return 0;
}