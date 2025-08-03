#include "../include/Lexer.h"
#include "../include/Parser.h"
#include "../include/AST.h"

using namespace llvm;

std::unique_ptr<LLVMContext> TheContext;
std::unique_ptr<IRBuilder<>> Builder;
std::unique_ptr<Module> TheModule;
// std::map<std::string, AllocaInst*> SymbolTable; // Stores all variables
std::map<std::string, std::map<std::string, AllocaInst*>> SymbolTable;
std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

// Optimization Vars
std::unique_ptr<FunctionPassManager> TheFPM;
std::unique_ptr<LoopAnalysisManager> TheLAM;
std::unique_ptr<FunctionAnalysisManager> TheFAM;
std::unique_ptr<CGSCCAnalysisManager> TheCGAM;
std::unique_ptr<ModuleAnalysisManager> TheMAM;
std::unique_ptr<PassInstrumentationCallbacks> ThePIC;
std::unique_ptr<StandardInstrumentations> TheSI;

// Codegen Definitions



Value *LemonAST::codegen(const std::string scope) {
    return nullptr;
}

Value *BinaryExprAST::codegen(const std::string scope) {
    Value *L = LHS->codegen();
    Value *R = RHS->codegen();

    if (!L || !R)
        return nullptr;
    
    switch (op) {
    case tok_add:
        return Builder->CreateFAdd(L, R, "addtmp");
    case tok_sub:
        return Builder->CreateFSub(L, R, "subtmp");
    case tok_mul:
        return Builder->CreateFMul(L, R, "multmp");
    case tok_div:
        return Builder->CreateFDiv(L, R, "divtmp");
    default:
        return LogErrorV("Invalid Binary Operator.");
    }
}

Value *NumberExprAST::codegen(const std::string scope) {
    return ConstantFP::get(*TheContext, APFloat(val));
}

Value *VariableExprAST::codegen(const std::string scope) {
    AllocaInst* A = SymbolTable[scope][varName];
    if (!A)
        return LogErrorV("Unknown variable name referenced.");
    
    return Builder->CreateLoad(A->getAllocatedType(), A, varName.c_str());
}

Value *CallExprAST::codegen(const std::string scope) {
    Function *calleeF = getFunction(callee);

    if (!calleeF) 
        return LogErrorV("Unknown function referenced.");

    if (calleeF->arg_size() != args.size())
        return LogErrorV("Incorrect # of arguments passed.");

    std::vector<Value *> argsValue;
    // Generating IR to evaluate all arguments first
    int arg_sz = args.size();
    for (int i = 0; i < arg_sz; ++i) {
        Value *evaluated = args[i]->codegen();
        if (!evaluated)
            return nullptr;
        argsValue.push_back(evaluated);
    }

    return Builder->CreateCall(calleeF, argsValue, "calltmp");
}

Value *VariableDeclStmt::codegen(const std::string scope) {
    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    Value *initVal;

    if (defBody) {
        initVal = defBody->codegen();
        if (!initVal)
            return nullptr;
    } else {
        // If not specified, default to 0.0.
        initVal = ConstantFP::get(*TheContext, APFloat(0.0)); 
    }

    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, varName);
    Builder->CreateStore(initVal, Alloca);

    SymbolTable[scope][varName] = Alloca;

    // Should return nullptr or something else? Alloca?
    return initVal;
}

Value *AssignmentStmt::codegen(const std::string scope) {
    Value *newVal = defBody->codegen();
    
    if (!newVal)
        return nullptr;

    Value *variable = SymbolTable[scope][varName];
    if (!variable)
        return LogErrorV("Unknown variable name referenced in assignment operator.");

    Builder->CreateStore(newVal, variable);
    return newVal;
}

Value *ReturnStmtAST::codegen(const std::string scope) {
    return nullptr;
}

Function *PrototypeAST::codegen(const std::string scope) {
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
    // Should return Function *
    // But since Function class inherits from Value, it should be fine :)
    auto &p = *proto; // Save a ref to use later on in code
    std::string functionScope = "_" + p.getName();
    FunctionProtos[proto->getName()] = std::move(proto);
    Function *TheFunction = getFunction(p.getName());

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
        for (auto &statement : functionBody) {
            Value *stmtVal = statement->codegen(functionScope);
            // Check if statement is a return?
            // Add return?
        }
        verifyFunction(*TheFunction);

        TheFPM->run(*TheFunction, *TheFAM);

        return TheFunction;
    }

    // Error occured, or doesn't have function body (?)
    TheFunction->eraseFromParent();
    return nullptr;
}

Value *ExternAST::codegen(const std::string scope) {
    return nullptr;
}


// Helper Function
Function *getFunction(std::string name) {
    // First, see if the function has already been added to the current module.
    if (auto *F = TheModule->getFunction(name))
        return F;

    // If not, check whether we can codegen the declaration from some existing
    // prototype.
    auto FI = FunctionProtos.find(name);
    if (FI != FunctionProtos.end())
        return FI->second->codegen();

    // If no existing prototype exists, return null.
    return nullptr;
}

AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
                                          StringRef varName) {
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                     TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(Type::getDoubleTy(*TheContext), nullptr, varName);
}