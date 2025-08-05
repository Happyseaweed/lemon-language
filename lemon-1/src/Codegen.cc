#include "../include/Lexer.h"
#include "../include/Parser.h"
#include "../include/AST.h"

using namespace llvm;

std::unique_ptr<LLVMContext> TheContext;
std::unique_ptr<IRBuilder<>> Builder; 

std::unique_ptr<IRBuilder<>> GlobalVariableBuilder;
std::unique_ptr<IRBuilder<>> MainBuilder;
std::unique_ptr<IRBuilder<>> FunctionBuilder;

std::unique_ptr<Module> TheModule;
std::map<std::string, std::map<std::string, AllocaInst*>> SymbolTable;  // Scope'd vars
std::map<std::string, GlobalVariable*> GlobalVariables;                 // Global vars.
std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;    // Function signatures

// Optimization Vars
std::unique_ptr<FunctionPassManager> TheFPM;
std::unique_ptr<LoopAnalysisManager> TheLAM;
std::unique_ptr<FunctionAnalysisManager> TheFAM;
std::unique_ptr<CGSCCAnalysisManager> TheCGAM;
std::unique_ptr<ModuleAnalysisManager> TheMAM;
std::unique_ptr<PassInstrumentationCallbacks> ThePIC;
std::unique_ptr<StandardInstrumentations> TheSI;

std::unique_ptr<LemonJIT> TheJIT;

int nextGlobalPriority = 0;

// Codegen Definitions
int dbug_cnt = 1;
void dbug() {
    std::string toPrint = "DBUG POINT: ";
    for (int i = 0; i < 10; ++i) toPrint += std::to_string(dbug_cnt);
    toPrint += "\n";
    dbug_cnt++;
    dbug_cnt %=10;
    fprintf(stderr, "%s", toPrint.c_str());    
}

Value *LemonAST::codegen(const std::string scope) {
    fprintf(stderr, "Lemon Codegen Started\n");
    for (auto &statement : statements) {
        auto *stmtIR = statement->codegen(scope);
        // fprintf(stderr, "\n\nStatment IR: \n");
        // if (stmtIR) stmtIR->print(errs());
    }
    return nullptr;
}

Value *BinaryExprAST::codegen(const std::string scope) {
    Value *L = LHS->codegen(scope);
    Value *R = RHS->codegen(scope);

    if (!L || !R)
        return nullptr;
  
    // TODO: Definitely need to refactor this.... use better methods...........
    IRBuilder<> *TmpBuilder = (scope == "_global") ? MainBuilder.get() : Builder.get();
        
    switch (op) {
    case tok_add:
        return TmpBuilder->CreateFAdd(L, R, "addtmp");
    case tok_sub:
        return TmpBuilder->CreateFSub(L, R, "subtmp");
    case tok_mul:
        return TmpBuilder->CreateFMul(L, R, "multmp");
    case tok_div:
        return TmpBuilder->CreateFDiv(L, R, "divtmp");
    default:
        return LogErrorV("Invalid Binary Operator.");
    }
}

Value *NumberExprAST::codegen(const std::string scope) {
    return ConstantFP::get(*TheContext, APFloat(val));
}

Value *VariableExprAST::codegen(const std::string scope) {
    AllocaInst* A = SymbolTable[scope][varName];
    GlobalVariable* GV = GlobalVariables[varName];

    if (A) {
        return Builder->CreateLoad(A->getAllocatedType(), A, varName.c_str());
    }
    else if (GV) {
        return Builder->CreateLoad(GV->getValueType(), GV, varName.c_str());
    }
    std::string errorStr = "Unknown variable name (" + varName + ") referenced in Scope: (" + scope + ").";
    return LogErrorV(errorStr.c_str());
}

Value *CallExprAST::codegen(const std::string scope) {
    Function *calleeF = getFunction(callee, scope);

    if (!calleeF) 
        return LogErrorV("Unknown function referenced.");

    if (calleeF->arg_size() != args.size())
        return LogErrorV("Incorrect # of arguments passed.");

    std::vector<Value *> argsValue;
    // Generating IR to evaluate all arguments first
    int arg_sz = args.size();
    for (int i = 0; i < arg_sz; ++i) {
        Value *evaluated = args[i]->codegen(scope);
        if (!evaluated)
            return nullptr;
        argsValue.push_back(evaluated);
    }
    if (scope == "_global")
        return MainBuilder->CreateCall(calleeF, argsValue, "calltmp");

    return Builder->CreateCall(calleeF, argsValue, "calltmp");
}

Value *VariableDeclStmt::codegen(const std::string scope) {
    if (scope == "_global")
        return codegen_global();

    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    Value *initVal;

    if (defBody) {
        initVal = defBody->codegen(scope);
        if (!initVal)
            return nullptr;
    } else {
        // If not specified, default to 0.0.
        initVal = ConstantFP::get(*TheContext, APFloat(0.0)); 
    }

    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, varName);
    Builder->CreateStore(initVal, Alloca);

    SymbolTable[scope][varName] = Alloca;

    return Alloca;
}

Value *VariableDeclStmt::codegen_global() {

    // Initialize with constant 0.0, then call initializer function on program startup
    Constant *dummy_constant = ConstantFP::get(*TheContext, APFloat(0.0));
    GlobalVariable *GV = new GlobalVariable(*TheModule, 
                                            Type::getDoubleTy(*TheContext), 
                                            false, 
                                            GlobalValue::ExternalLinkage, 
                                            dummy_constant, 
                                            varName
    );
    
    // Build init function
    if (defBody) {

        // Create initializer function of void() return (similar to prototype)
        // TODO: Put this into its own helper function(s)
        std::string initFuncScope = "_init_global_" + varName;

        FunctionType *FT = FunctionType::get(
            Type::getVoidTy(*TheContext), 
            false
        );
        
        // Internal linkage, no need for cross-module
        Function *F = Function::Create(
            FT, 
            Function::InternalLinkage,     
            initFuncScope, 
            TheModule.get()
        );
        
        // Basic block:
        BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", F);

        // User tmp builder to build inside init func.
        std::unique_ptr<IRBuilder<>> TmpBuilder = std::make_unique<IRBuilder<>>(BB);  
        swap(TmpBuilder, Builder); // Swap the old builder with the new one.

        Builder->SetInsertPoint(BB);

        Value *initVal = defBody->codegen(initFuncScope);
        if (!initVal) {
            F->eraseFromParent();
            return nullptr;
        }

        // Store to global:
        Builder->CreateStore(initVal, GV);
        Builder->CreateRetVoid();

        swap(TmpBuilder, Builder); // Swap back the old builder.

        // llvm::appendToGlobalCtors 
        // Referenced from: https://llvm.org/doxygen/ModuleUtils_8h.html

        appendToGlobalCtors(*TheModule, F, nextGlobalPriority++);

        // Add it to table
        GlobalVariables[varName] = GV;
    }

    return GV;
}

Value *AssignmentStmt::codegen(const std::string scope) {
    Value *newVal = defBody->codegen(scope);
    
    if (!newVal)
        return nullptr;

    Value *variable = SymbolTable[scope][varName];
    if (!variable)
        variable = GlobalVariables[varName];

    if (!variable)
        return LogErrorV("Unknown variable name referenced in assignment operator.");

    if (scope == "_global") 
        MainBuilder->CreateStore(newVal, variable);
    else    
        Builder->CreateStore(newVal, variable);

    return newVal;
}

Value *ReturnStmtAST::codegen(const std::string scope) {
    return nullptr;
}

Value *ExpressionStmtAST::codegen(const std::string scope) {
    return expr->codegen(scope);
}

Function *PrototypeAST::codegen(const std::string scope) {
    fprintf(stderr, "Prototype codegen called in: (%s)\n", scope.c_str());
    std::vector<Type*> doubles(args.size(), Type::getDoubleTy(*TheContext));

    FunctionType *FT = 
        FunctionType::get(Type::getDoubleTy(*TheContext), doubles, false);

    Function *F = 
        Function::Create(FT, Function::ExternalLinkage, name, TheModule.get());
    
    unsigned idx = 0;
    for (auto &arg : F->args()) 
        arg.setName(args[idx++]); // Transfer arg names to LLVM.

    return F;
}

Value *FunctionAST::codegen(const std::string scope) {
    fprintf(stderr, "Function Codegen\n");
    // Should return Function *
    // But since Function class inherits from Value, it should be fine :)
    auto &p = *proto; // Save a ref to use later on in code
    std::string functionScope = "_" + p.getName();

    FunctionProtos[proto->getName()] = std::move(proto);
    Function *TheFunction = getFunction(p.getName(), scope);

    if (!TheFunction)
        return nullptr;
    
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB); // Update builder to insert into function

    // Adding arguments to function scope
    for (auto &arg : TheFunction->args()) {
        AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, arg.getName());

        Builder->CreateStore(&arg, Alloca);
        
        SymbolTable[functionScope][arg.getName().str()] = Alloca;
        // printf("Function: (%s) has argument: (%s)\n", proto->getName().c_str(), arg.getName().str().c_str());
    }

    // Generating body
    if (functionBody.size() > 0) {
        for (int i = 0; i < functionBody.size(); ++i) {
            Value *stmtVal = functionBody[i]->codegen(functionScope);

            // Assume a return is last statement...
            if (i == functionBody.size() - 1) {
                Builder->CreateRet(stmtVal);    
            }
        }

        // for (auto &statement : functionBody) {
        //     fprintf(stderr, "%s\n", functionScope.c_str());
        //     Value *stmtVal = statement->codegen(functionScope);
        //     // Check if statement is a return?
        //     // Add return?
        // }
        verifyFunction(*TheFunction);

        // TheFPM->run(*TheFunction, *TheFAM);

        return TheFunction;
    }

    // Error occured, or doesn't have function body (?)
    TheFunction->eraseFromParent();
    return nullptr;
}

Value *ExternAST::codegen(const std::string scope) {
    // Should always be global scope.
    auto &p = proto;

    FunctionProtos[proto->getName()] = std::move(proto);

    return nullptr;
} 


// Helper Function
Function *getFunction(std::string name, std::string scope) {
    // First, see if the function has already been added to the current module.
    if (auto *F = TheModule->getFunction(name))
        return F;

    // If not, check whether we can codegen the declaration from some existing
    // prototype.
    // This codegen's the prototypeAST.
    auto FI = FunctionProtos.find(name);
    if (FI != FunctionProtos.end())
        return FI->second->codegen(scope);

    // If no existing prototype exists, return null.
    return nullptr;
}

AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
                                          StringRef varName) {
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                     TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(Type::getDoubleTy(*TheContext), nullptr, varName);
}