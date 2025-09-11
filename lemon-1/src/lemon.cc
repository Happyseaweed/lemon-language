#include "../include/Parser.h"
#include "../include/AST.h"
#include "../include/Lexer.h"


static ExitOnError ExitOnErr;
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
    fprintf(stderr, "Print: ");
    fprintf(stderr, "%f\n", X);
    return 0;
}

// ============================================================================
//                                  Main
// ============================================================================

int REPL_MODE = 0;

void InitializeModule() {
    // Open a new context and module.
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("LEMON JIT", *TheContext);
    TheModule->setDataLayout(TheJIT->getDataLayout());

    // Create a new builder for the module.
    Builder = std::make_unique<IRBuilder<>>(*TheContext);

    // 3 insertion points.
    GlobalVariableBuilder = std::make_unique<IRBuilder<>>(*TheContext);
    MainBuilder = std::make_unique<IRBuilder<>>(*TheContext);
    FunctionBuilder = std::make_unique<IRBuilder<>>(*TheContext);

    // Optimizations
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
    // Eliminate Common SubExpressions.
    TheFPM->addPass(GVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    TheFPM->addPass(SimplifyCFGPass());

    // mem2reg passes
    TheFPM->addPass(PromotePass());
    TheFPM->addPass(InstCombinePass());
    TheFPM->addPass(ReassociatePass());

    // ADCE passes
    TheFPM->addPass(ADCEPass());
    TheFPM->addPass(DSEPass());

    // Register analysis passes used in these transform passes.
    PassBuilder PB;
    PB.registerModuleAnalyses(*TheMAM);
    PB.registerFunctionAnalyses(*TheFAM);
    PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}

void runGlobalConstructors(std::vector<std::string> constructors) {
    if (constructors.empty()) {
        fprintf(stderr, "No global constructors found.\n");
        return;
    }

    for (auto &ctorName : constructors) {
        auto Sym = ExitOnErr(TheJIT->lookup(ctorName));
        void (*FnPtr)() = Sym.toPtr<void (*)()>();
        FnPtr();
    }
}

std::vector<std::string> findGlobalConstructors(GlobalVariable *GlobalCtors) {
    std::vector<std::string> constructors;

    if (!GlobalCtors)
        return constructors;

    auto *ctorArray = cast<ConstantArray>(GlobalCtors->getInitializer());

    for (auto &op : ctorArray->operands()) {
        auto *CtorStruct = llvm::dyn_cast<llvm::ConstantStruct>(op);
        if (!CtorStruct || CtorStruct->getNumOperands() < 2)
            continue;

        auto *CtorFunc = llvm::dyn_cast<llvm::Function>(CtorStruct->getOperand(1));
        if (!CtorFunc)
            continue;

        constructors.push_back(CtorFunc->getName().str());
    }    
    
    return constructors;    
}

void runLemon() {
    getNextToken();
    while (true) {
        switch (curTok) {

        case tok_eof:
            return;

        default:
            auto result = Parse();

            // Make main func.
            FunctionType *FT = 
                FunctionType::get(Type::getDoubleTy(*TheContext), false);        
            
            Function *F =
                Function::Create(FT, Function::ExternalLinkage, "lemon_main", TheModule.get());
            
            BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", F);
            MainBuilder->SetInsertPoint(BB);
           
            result->showAST(); // Print AST for debugging.
            
            result->codegen();

            // Optimizations:
            TheFPM->run(*F, *TheFAM);

            // Saving LLVM IR to a file.
            std::error_code EC;
            raw_fd_ostream out("./output.ll", EC, sys::fs::OF_None);
            
            if (EC) {
                errs() << "Error opening file: " << EC.message() << "\n";
                return;
            }
            
            TheModule->print(out, nullptr);
          
            // JIT Execution (using this as "AOT" compiler for now...)
            // fprintf(stderr, "🍋 Lemon Executing...\n");
            
            // Getting global variable constructors.
            GlobalVariable *GlobalCtors = TheModule->getGlobalVariable("llvm.global_ctors");
            std::vector<std::string> GlobalConstructorFunctions = findGlobalConstructors(GlobalCtors);
            
            // Creating resource tracker and loading context on to JIT
            auto RT = TheJIT->getMainJITDylib().getDefaultResourceTracker();
            auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));
            ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
            InitializeModule();

            // Run global constructors to initialize global variables, before lemon_main.
            // DEPRECATED, now calling inits() in lemon_main
            // runGlobalConstructors(GlobalConstructorFunctions);

            // Executing main()
            auto ExprSymbol = ExitOnErr(TheJIT->lookup("lemon_main"));
            double (*FP)() = ExprSymbol.toPtr<double (*)()>();
            printf("\n\n\nLemon Execution: \n");
            FP();
            // fprintf(stderr, "Evaluated to %f\n\n\n", FP());
            
            // Dumping JITDylib symbol table.
            // TheJIT->getMainJITDylib().dump(errs());
            
            ExitOnErr(RT->remove());
            
            return;
        }
    }
}

void runLemonREPL() {
    fprintf(stderr, "LEMON> ");
    getNextToken();
    while (true) {
        switch (curTok) {
        case tok_eof:
            break;
        
        default:
            auto curLine = Parse();

            // Make current block function
            FunctionType *FT =
                FunctionType::get(Type::getDoubleTy(*TheContext), false);
            Function *F =
                Function::Create(FT, Function::ExternalLinkage, "lemon_block", TheModule.get());
            
            BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", F);
            
            // TBC
        }
    }
}

int main(int argc, char **argv) {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    
    if (argc > 1 && *argv[1] == '1') {
        REPL_MODE = 1;
    }
    
    // comparison ops
    operatorPrecedence[tok_lt] = 10; 
    operatorPrecedence[tok_gt] = 10; 
    operatorPrecedence[tok_le] = 10; 
    operatorPrecedence[tok_ge] = 10; 
    operatorPrecedence[tok_eq] = 10;

    // expr ops
    operatorPrecedence[tok_add] = 20;
    operatorPrecedence[tok_sub] = 30;
    operatorPrecedence[tok_mul] = 40;
    operatorPrecedence[tok_div] = 40;
    
    TheJIT = ExitOnErr(LemonJIT::Create());
    
    InitializeModule();
    
    if (REPL_MODE)
        runLemonREPL();
    else
        runLemon();

    return 0;
}
