#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
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

#include "./LemonJIT.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;
using namespace llvm::orc;
static ExitOnError ExitOnErr;

// LLVM vars
static std::unique_ptr<LLVMContext> TheContext;     // Contains core LLVM datastructures
static std::unique_ptr<IRBuilder<>> Builder;        // Used for generating IR instructions
static std::unique_ptr<Module> TheModule;           // Conains all generated IR
static std::map<std::string, Value *> NamedValues;  // Symbol table for the code and its generated IR

static std::unique_ptr<FunctionPassManager> TheFPM;
static std::unique_ptr<LoopAnalysisManager> TheLAM;
static std::unique_ptr<FunctionAnalysisManager> TheFAM;
static std::unique_ptr<CGSCCAnalysisManager> TheCGAM;
static std::unique_ptr<ModuleAnalysisManager> TheMAM;
static std::unique_ptr<PassInstrumentationCallbacks> ThePIC;
static std::unique_ptr<StandardInstrumentations> TheSI;

static std::unique_ptr<LemonJIT> TheJIT;

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
    tok_in = -10
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
        if (IdentifierStr == "if")
            return tok_if;
        if (IdentifierStr == "then")
            return tok_then;
        if (IdentifierStr == "else")
            return tok_else;
        if (IdentifierStr == "for")
            return tok_for;
        if (IdentifierStr == "in")
            return tok_in;

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
        fprintf(stderr, "Number Expr Codegen\n");
        return ConstantFP::get(*TheContext, APFloat(Val));
    }
};

class VariableExprAST : public ExprAST {
    std::string Name;
public:
    VariableExprAST(const std::string &Name): Name(Name) {}
    Value *codegen() override {
        fprintf(stderr, "Variable Expr Codegen\n");
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

        fprintf(stderr, "Binary Expr Codegen\n");
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

class IfExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Cond, Then, Else;
public:
    IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then, 
              std::unique_ptr<ExprAST> Else)
        : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
    
    Value *codegen() override;
};

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

class PrototypeAST {
    std::string Name;
    std::vector<std::string> Args;

public:
    PrototypeAST(const std::string &Name, std::vector<std::string> Args)
        : Name(Name), Args(std::move(Args)) {}
    
    const std::string &getName() const { return Name; }
    Function *codegen() {
        fprintf(stderr, "Prototype Codegen\n");
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

static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
Function *getFunction(std::string Name) {
    // First, see if the function has already been added to the current module.
    if (auto *F = TheModule->getFunction(Name))
        return F;

    // If not, check whether we can codegen the declaration from some existing
    // prototype.
    auto FI = FunctionProtos.find(Name);
    if (FI != FunctionProtos.end())
        return FI->second->codegen();

    // If no existing prototype exists, return null.
    return nullptr;
}

class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST> > Args;

public:
    CallExprAST(const std::string &Callee, 
                std::vector<std::unique_ptr<ExprAST> > Args)
        : Callee(Callee), Args(std::move(Args)) {}
    Value *codegen() override {
        fprintf(stderr, "Call Expr Codegen\n");
        // Look up the name in the global module table.
        Function *CalleeF = getFunction(Callee);
        // Function *CalleeF = TheModule->getFunction(Callee);

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



class FunctionAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;
public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto, 
                std::unique_ptr<ExprAST> Body)
        : Proto(std::move(Proto)), Body(std::move(Body)) {}

    Function *codegen() {
        fprintf(stderr, "Function Codegen\n"); 
        // First, check for an existing function from a previous 'extern' declaration.
        
        auto &P = *Proto;
        FunctionProtos[Proto->getName()] = std::move(Proto);
        Function *TheFunction = getFunction(P.getName());
        
        if (!TheFunction)
            return nullptr;

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

            // Optimization, wow this is literally magic!!!
            TheFPM->run(*TheFunction, *TheFAM);

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


static std::unique_ptr<ExprAST> ParseIfExpr() {
    getNextToken(); // Consume 'if'

    auto Cond = ParseExpression();
    if (!Cond) 
        return nullptr;

    if (CurTok != tok_then)
        return LogError("Expected 'then' ");
    getNextToken(); // Consume 'then'

    auto Then = ParseExpression();
    if (!Then)
        return nullptr;

    if (CurTok != tok_else)
        return LogError("expected 'else'");

    getNextToken();

    auto Else = ParseExpression();
    if (!Else)
        return nullptr;

    return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then), 
                                       std::move(Else));
}

static std::unique_ptr<ExprAST> ParseForExpr() {
    getNextToken();

    if (CurTok != tok_identifier)
        return LogError("expected identifier after for");

    std::string IdName = IdentifierStr;
    getNextToken();  // eat identifier.

    if (CurTok != '=')
        return LogError("expected '=' after for");
    getNextToken();  // eat '='.

    auto Start = ParseExpression();
    if (!Start)
        return nullptr;
    if (CurTok != ',')
        return LogError("expected ',' after for start value");
    getNextToken();

    auto End = ParseExpression();
    if (!End)
        return nullptr;

    // The step value is optional.
    std::unique_ptr<ExprAST> Step;
    if (CurTok == ',') {
        getNextToken();
        Step = ParseExpression();
        if (!Step)
        return nullptr;
    }

    if (CurTok != tok_in)
        return LogError("expected 'in' after for");
    getNextToken();  // eat 'in'.

    auto Body = ParseExpression();
    if (!Body)
        return nullptr;

    return std::make_unique<ForExprAST>(IdName, std::move(Start),
                                        std::move(End), std::move(Step),
                                        std::move(Body));
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
        case tok_if:
            return ParseIfExpr();
        case tok_for:
            return ParseForExpr();
    }
}

// ============================================================================
Value *IfExprAST::codegen() {
    Value *CondV = Cond->codegen();

    if (!CondV)
        return nullptr;

    CondV = Builder->CreateFCmpONE(
        CondV, ConstantFP::get(*TheContext, APFloat(0.0)), "ifcond");
    
    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *ThenBB = 
        BasicBlock::Create(*TheContext, "then", TheFunction);
    BasicBlock *ElseBB = BasicBlock::Create(*TheContext, "else");
    BasicBlock *MergeBB = BasicBlock::Create(*TheContext, "ifcont");

    Builder->CreateCondBr(CondV, ThenBB, ElseBB);

    Builder->SetInsertPoint(ThenBB);

    Value *ThenV = Then->codegen();
    if (!ThenV)
        return nullptr;
    
    Builder->CreateBr(MergeBB);
    ThenBB = Builder->GetInsertBlock();

    // Else block
    TheFunction->insert(TheFunction->end(), ElseBB);
    Builder->SetInsertPoint(ElseBB);

    Value *ElseV = Else->codegen();
    if (!ElseV)
        return nullptr;
    
    Builder->CreateBr(MergeBB);
    ElseBB = Builder->GetInsertBlock();

    TheFunction->insert(TheFunction->end(), MergeBB);
    Builder->SetInsertPoint(MergeBB);
    PHINode *PN =
        Builder->CreatePHI(Type::getDoubleTy(*TheContext), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);
    return PN;
}

Value *ForExprAST::codegen() {
    // Start
    Value *StartVal = Start->codegen();
    if (!StartVal)
        return nullptr;
    
    // BB for start of loop body
    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *PreheaderBB = Builder->GetInsertBlock();
    BasicBlock *LoopBB =
        BasicBlock::Create(*TheContext, "loop", TheFunction);

    Builder->CreateBr(LoopBB);

    // Start insertion in LoopBB
    Builder->SetInsertPoint(LoopBB);
    PHINode *Variable = Builder->CreatePHI(Type::getDoubleTy(*TheContext),
                                           2, VarName);
    
    Variable->addIncoming(StartVal, PreheaderBB);

    Value *OldVal = NamedValues[VarName];
    NamedValues[VarName] = Variable;

    if (!Body->codegen())
        return nullptr;

    Value *StepVal = nullptr;
    if (Step) {
        StepVal = Step->codegen();
        if (!StepVal)
            return nullptr;
    } else {
        StepVal = ConstantFP::get(*TheContext, APFloat(1.0));
    }

    Value *NextVar = Builder->CreateFAdd(Variable, StepVal, "nextvar");

    Value *EndCond = End->codegen();
    if (!EndCond)
        return nullptr;
    
    // Exit value of loop
    EndCond = Builder->CreateFCmpONE(
        EndCond, ConstantFP::get(*TheContext, APFloat(0.0)), "loopcond");

    BasicBlock *LoopEndBB = Builder->GetInsertBlock();
    BasicBlock *AfterBB =
        BasicBlock::Create(*TheContext, "afterloop", TheFunction);

    Builder->CreateCondBr(EndCond, LoopBB, AfterBB);

    Builder->SetInsertPoint(AfterBB);

    Variable->addIncoming(NextVar, LoopEndBB);

    if (OldVal)
        NamedValues[VarName] = OldVal;
    else
        NamedValues.erase(VarName);
    
    return Constant::getNullValue(Type::getDoubleTy(*TheContext));
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

static void InitializeModuleAndManager(void);

static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    fprintf(stderr, "Parsed a function definition.\n");
    if (auto *FnIR = FnAST->codegen()) {
        fprintf(stderr, "Read function definition: ");
        FnIR->print(errs());
        fprintf(stderr, "\n");
        ExitOnErr(TheJIT->addModule(
            ThreadSafeModule(std::move(TheModule), std::move(TheContext))));
        InitializeModuleAndManager();
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleExtern() {
  if (auto FnAST = ParseExtern()) {
    fprintf(stderr, "Parsed an extern\n");
    if (auto *FnIR = FnAST->codegen()) {
        fprintf(stderr, "Read extern: ");
        FnIR->print(errs());
        fprintf(stderr, "\n");
        FunctionProtos[FnAST->getName()] = std::move(FnAST);
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}


static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = ParseTopLevelExpr()) {
    fprintf(stderr, "Parsed a top-level expr\n");
    if (FnAST->codegen()) {
        // Create a ResourceTracker to track JIT'd memory allocated to our
        // anonymous expression -- that way we can free it after executing.
        auto RT = TheJIT->getMainJITDylib().createResourceTracker();

        auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));
        ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
        InitializeModuleAndManager();

        // Search the JIT for the __anon_expr symbol.
        auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));
        // assert(ExprSymbol && "Function not found");

        // Get the symbol's address and cast it to the right type (takes no
        // arguments, returns a double) so we can call it as a native function.
        double (*FP)() = ExprSymbol.toPtr<double (*)()>();
        fprintf(stderr, "Evaluated to %f\n", FP());

        // Delete the anonymous expression module from the JIT.
        ExitOnErr(RT->remove());
    }
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

static void InitializeModuleAndManager(void) {
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("Lemon JIT", *TheContext);
    TheModule->setDataLayout(TheJIT->getDataLayout());

    Builder = std::make_unique<IRBuilder<> >(*TheContext);

    // Create new pass and analysis managers.
    TheFPM = std::make_unique<FunctionPassManager>();
    TheLAM = std::make_unique<LoopAnalysisManager>();
    TheFAM = std::make_unique<FunctionAnalysisManager>();
    TheCGAM = std::make_unique<CGSCCAnalysisManager>();
    TheMAM = std::make_unique<ModuleAnalysisManager>();

    ThePIC = std::make_unique<PassInstrumentationCallbacks>();
    TheSI = std::make_unique<StandardInstrumentations>(*TheContext,
                                                        /*DebugLogging*/ true);
    TheSI->registerCallbacks(*ThePIC, TheMAM.get());

    // Add transform passes.
    // Do simple "peephole" optimizations and bit-twiddling optimizations.
    TheFPM->addPass(InstCombinePass());
    // Reassociate expressions.
    TheFPM->addPass(ReassociatePass());
    // Eliminate Common SubExpressions.
    TheFPM->addPass(GVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    TheFPM->addPass(SimplifyCFGPass());

    // // Register analysis passes used in these transform passes.
    PassBuilder PB;
    PB.registerModuleAnalyses(*TheMAM);
    PB.registerFunctionAnalyses(*TheFAM);
    PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}

int main() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    // Smaller number = lower precedence.
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 30;
    BinopPrecedence['*'] = 40;

    fprintf(stderr, "ready> ");
    getNextToken();

    TheJIT = ExitOnErr(LemonJIT::Create());
    // TheJIT = std::make_unique<LemonJIT>();

    InitializeModuleAndManager();

    MainLoop();

    return 0;
}