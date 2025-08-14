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
std::map<std::string, std::map<std::string, AllocaInst*>> SymbolTable;  // SymbolTable for each scope.
std::stack<std::string> ScopeStack;
std::map<std::string, GlobalVariable*> GlobalVariables;                 // Global variables
std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;    // Function signatures

int LoopScopeCounter;

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
    // fprintf(stderr, "# Lemon Codegen Started\n");
    const int totalStatements = statements.size();
    int i = 0;
    for (auto &statement : statements) {
        Value *stmtVal = statement->codegen(scope);
        if (i == totalStatements-1) {
            MainBuilder->CreateRet(stmtVal);
        }
        i++;
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
    case tok_lt:
        L = TmpBuilder->CreateFCmpULT(L, R, "cmptmp_lt");
        return TmpBuilder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp_lt");
    case tok_gt:
        L = TmpBuilder->CreateFCmpUGT(L, R, "cmptmp_gt");
        return TmpBuilder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp_gt");
    case tok_le:
        L = TmpBuilder->CreateFCmpULE(L, R, "cmptmp_le");
        return TmpBuilder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp_le");
    case tok_ge:
        L = TmpBuilder->CreateFCmpUGE(L, R, "cmptmp_ge");
        return TmpBuilder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp_ge");
    case tok_eq:
        L = TmpBuilder->CreateFCmpUEQ(L, R, "cmptmp_eq");
        return TmpBuilder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp_eq");

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
        if (scope == "_global")
            return MainBuilder->CreateLoad(A->getAllocatedType(), A, varName.c_str());
        return Builder->CreateLoad(A->getAllocatedType(), A, varName.c_str());
    }
    else if (GV) {
        if (scope == "_global")
            return MainBuilder->CreateLoad(GV->getValueType(), GV, varName.c_str());
        return Builder->CreateLoad(GV->getValueType(), GV, varName.c_str());
    }
    std::string errorStr = "Unknown variable name (" + varName + ") referenced in Scope: (" + scope + ").";
    return LogErrorV(errorStr.c_str());
}

Value *TensorExprAST::codegen(const std::string scope) {
    fprintf(stderr, "Tensor Expr AST Codegen()\n");

    return nullptr;
}

Value *SubscriptExprAST::codegen(const std::string scope) {
    // Generates code and evaluates subscripts to integer values.
    // Check bounds against stored tensor at compile time
    
    return nullptr;
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

    if (scope == "_global") {
        return MainBuilder->CreateCall(calleeF, argsValue, "calltmp");
    }

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
            Function::ExternalLinkage,     
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
        
        // Optimizations
        TheFPM->run(*F, *TheFAM);

        // llvm::appendToGlobalCtors 
        // Referenced from: https://llvm.org/doxygen/ModuleUtils_8h.html
        // DEACTIVATED, currently init'ing global vars via function calls in lemon_main
        // appendToGlobalCtors(*TheModule, F, nextGlobalPriority++);

        // Add it to table
        GlobalVariables[varName] = GV;
        
        // Call init function in main
        std::string initFuncName = initFuncScope;
        Function *calleeF = getFunction(initFuncName);  // Should get directly from TheModule
        MainBuilder->CreateCall(calleeF, std::vector<Value*>(), "init_calltmp");
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
    Value *retV = retBody->codegen(scope);
    return retV;
}

Value *ExpressionStmtAST::codegen(const std::string scope) {
    return expr->codegen(scope);
}

Value *IfStmtAST::codegen(const std::string scope) {
    Value *condV = cond->codegen(scope);

    if (!condV)
        return nullptr;
    
    // TODO: Need a better way to handle builders... this is tedious!
    if (scope == "_global")             
        swap(Builder, MainBuilder);

    condV = Builder->CreateFCmpONE(
        condV, 
        ConstantFP::get(*TheContext, APFloat(0.0)), 
        "ifcond"
    );

    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *ThenBB = 
        BasicBlock::Create(*TheContext, "then", TheFunction);
    BasicBlock *ElseBB = 
        BasicBlock::Create(*TheContext, "else");
    BasicBlock *MergeBB = 
        BasicBlock::Create(*TheContext, "ifcont");
    
    Builder->CreateCondBr(condV, ThenBB, ElseBB);

    // Emitting THEN block
    Builder->SetInsertPoint(ThenBB);

    if (scope == "_global")
        swap(Builder, MainBuilder);
    
    for (auto &stmt : thenBody) {
        Value *thenStmtV = stmt->codegen(scope);
        if (!thenStmtV)
            return nullptr;
    }
    
    if (scope == "_global")
        swap(Builder, MainBuilder);

    // After THEN block, jump to MergeBB
    Builder->CreateBr(MergeBB);
    ThenBB = Builder->GetInsertBlock(); // Update ThenBB

    // Emitting ELSE block
    TheFunction->insert(TheFunction->end(), ElseBB);
    Builder->SetInsertPoint(ElseBB);

    if (scope == "_global")
        swap(Builder, MainBuilder);
    
    for (auto &stmt : elseBody) {
        Value *elseStmtV = stmt->codegen(scope);
        if (!elseStmtV)
            return nullptr;
    }

    if (scope == "_global")
        swap(Builder, MainBuilder);
    
    Builder->CreateBr(MergeBB);
    ElseBB = Builder->GetInsertBlock(); // Update ElseBB

    TheFunction->insert(TheFunction->end(), MergeBB);
    Builder->SetInsertPoint(MergeBB);

    // Phinodes:
    PHINode *PN = 
        Builder->CreatePHI(Type::getDoubleTy(*TheContext), 2, "iftmp");

    PN->addIncoming(ConstantFP::get(*TheContext, APFloat(0.0)), ThenBB);
    PN->addIncoming(ConstantFP::get(*TheContext, APFloat(0.0)), ElseBB);
    
    if (scope == "_global")
        swap(Builder, MainBuilder);
    
    return PN;
}

Value *ForStmtAST::codegen(const std::string scope) {
    // if global scope, generate local vars within main
    // if in func scope, generate local vars within func

    // Check against local vars first, if same name exists, use it.
    // Otherwise create new local var. 
    // lemon_main should have NO local-only variables, other than global vars 

    // inits start
    // checks cond
    // loop:
    //      loop stuff
    //      checks cond
    // afterloop:
    //

    // Create iterator start value;
    Value *startV = start->codegen(scope);
    if (!startV)
        return nullptr;
    
    if (scope == "_global")
        swap(Builder, MainBuilder);
    
    // Get parent block
    Function* F = Builder->GetInsertBlock()->getParent();
    
    AllocaInst *Alloca = CreateEntryBlockAlloca(F, iterator);
    Builder->CreateStore(startV, Alloca);
    SymbolTable[scope][iterator] = Alloca;

    // Basic blocks
    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "loop", F);
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "afterloop", F);
    
    if (scope == "_global")
        swap(Builder, MainBuilder);
    
        // Calculate step value
    Value *stepVal = step->codegen(scope);

    // Evaluate expression to a value
    Value *endVal = end->codegen(scope);
    if (!endVal)
        return nullptr;

    
    if (scope == "_global")
        swap(Builder, MainBuilder);

    // Compare current value & branch
    Value *curVal = Builder->CreateLoad(Alloca->getAllocatedType(), Alloca, iterator.c_str());
    Value *endCond = Builder->CreateFCmpULT(curVal, endVal, "loopcond");
    Builder->CreateCondBr(endCond, LoopBB, AfterBB);

    // Loop body:
    Builder->SetInsertPoint(LoopBB);

    if (scope == "_global")
        swap(Builder, MainBuilder);
    
    for (auto &stmt : forBody) {
        stmt->codegen(scope);
    }
    
    if (scope == "_global")
        swap(Builder, MainBuilder);

    // Increment iterator
    curVal = Builder->CreateLoad(Alloca->getAllocatedType(), Alloca, iterator.c_str());
    Value *nextVal = Builder->CreateFAdd(curVal, stepVal, "nextval");
    Builder->CreateStore(nextVal, Alloca);
    
    // Check termination condition
    endCond = Builder->CreateFCmpULT(nextVal, endVal, "loopcond");
    Builder->CreateCondBr(endCond, LoopBB, AfterBB);

    Builder->SetInsertPoint(AfterBB);

    if (scope == "_global")
        swap(Builder, MainBuilder);

    return nullptr;
}

Function *PrototypeAST::codegen(const std::string scope) {
    // fprintf(stderr, "Prototype codegen called in: (%s)\n", scope.c_str());
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
    // fprintf(stderr, "Function Codegen\n");
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
    }

    // Generating body
    if (functionBody.size() > 0) {
        for (int i = 0; i < functionBody.size(); ++i) {
            Value *stmtVal = functionBody[i]->codegen(functionScope);

            // Check if is return statement:
            if (ReturnStmtAST* dPtr = dynamic_cast<ReturnStmtAST*>(functionBody[i].get())) {
                Builder->CreateRet(stmtVal);
            }
        }
        Builder->CreateRet(ConstantFP::get(*TheContext, APFloat(0.0)));

        verifyFunction(*TheFunction);

        // Optimizations
        TheFPM->run(*TheFunction, *TheFAM);

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

std::string generateLoopScope() {
    return "_Loop_" + std::to_string(LoopScopeCounter++);
}