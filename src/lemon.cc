#include "../include/Lexer.h"
#include "../include/AST.h"
#include "../include/Parser.h"

static ExitOnError ExitOnErr;
static int verbose = 0;
static int REPL_MODE = 0;

static void InitializeModuleAndManager(void);

static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    fprintf(stderr, "Parsed a function definition.\n");
    if (auto *FnIR = FnAST->codegen()) {
        if (verbose) fprintf(stderr, "Read function definition: ");
        if (verbose) FnIR->print(errs());
        if (verbose) fprintf(stderr, "\n");
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
    if (verbose) fprintf(stderr, "Parsed an extern\n");
    if (auto *FnIR = FnAST->codegen()) {
        if (verbose) fprintf(stderr, "Read extern: ");
        if (verbose) FnIR->print(errs());
        if (verbose) fprintf(stderr, "\n");
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
    if (verbose) fprintf(stderr, "Parsed a top-level expr\n");
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

static void HandleTopLevelStatement() {
    fprintf(stderr, "Handling Top Level Statement.\n");
    if (auto Stmt = ParseStatement()) {
        // Proto
        auto Proto = std::make_unique<PrototypeAST>("__anon_stmt", std::vector<std::string>());

        auto FnAST = std::make_unique<FunctionAST>(std::move(Proto), std::move(Stmt));

        if (FnAST->codegen()) {
            // Create a ResourceTracker to track JIT'd memory allocated to our
            // anonymous stmt -- that way we can free it after executing.
            auto RT = TheJIT->getMainJITDylib().createResourceTracker();

            auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));
            ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
            InitializeModuleAndManager();

            // Search JIT for __anon_stmt symbol
            auto StmtSymbol = ExitOnErr(TheJIT->lookup("__anon_stmt"));

            // Get the symbol's address and cast it to the right type (no returns) 
            // so we can call it as a native function.
            void (*FP)() = StmtSymbol.toPtr<void (*)()>();

            FP(); // Execute stmt block

            // Delete the anonymous expression module from the JIT.
            ExitOnErr(RT->remove());
        }
    } else {
        getNextToken();
    }
}

static void MainLoop() {
    while(true) {
        if (REPL_MODE) fprintf(stderr, "ready> ");
        switch (CurTok)  {

            case tok_eof:
                return;
            case tok_def:
                HandleDefinition();
                break;
            case tok_extern:
                HandleExtern();
                break;
           
            case tok_semi:
                getNextToken();
                break;

            case tok_lbrace:
            case tok_rbrace:
            case tok_if:
            case tok_for:
            case tok_return:
            case tok_var:
                HandleTopLevelStatement(); 
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

    // mem2reg passes
    TheFPM->addPass(PromotePass());
    TheFPM->addPass(InstCombinePass());
    TheFPM->addPass(ReassociatePass());

    // // Register analysis passes used in these transform passes.
    PassBuilder PB;
    PB.registerModuleAnalyses(*TheMAM);
    PB.registerFunctionAnalyses(*TheFAM);
    PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}


// ============================================================================
//          Mock "library" functions to be "extern'd" in user code
// ============================================================================

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}

// ============================================================================
//                                  Main
// ============================================================================

int main(int argc, char *argv[]) {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    // Smaller number = lower precedence.
    BinopPrecedence['='] = 2;
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 30;
    BinopPrecedence['*'] = 40;

    if (REPL_MODE) fprintf(stderr, "ready> ");
    getNextToken();

    TheJIT = ExitOnErr(LemonJIT::Create());

    InitializeModuleAndManager();

    MainLoop();

    return 0;
}