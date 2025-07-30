
// ============================================================================
//                  AST Implementations for Lemon Language
// ============================================================================
#include "../include/AST.h"

std::unique_ptr<LLVMContext> TheContext;     // Contains core LLVM datastructures
std::unique_ptr<IRBuilder<>> Builder;        // Used for generating IR instructions
std::unique_ptr<Module> TheModule;           // Conains all generated IR
std::map<std::string, AllocaInst*> NamedValues;  // Symbol table for the code and its generated IR

std::unique_ptr<FunctionPassManager> TheFPM;
std::unique_ptr<LoopAnalysisManager> TheLAM;
std::unique_ptr<FunctionAnalysisManager> TheFAM;
std::unique_ptr<CGSCCAnalysisManager> TheCGAM;
std::unique_ptr<ModuleAnalysisManager> TheMAM;
std::unique_ptr<PassInstrumentationCallbacks> ThePIC;
std::unique_ptr<StandardInstrumentations> TheSI;

std::unique_ptr<LemonJIT> TheJIT;
std::map<char, int> BinopPrecedence;

std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

Value *NumberExprAST::codegen() {
    return ConstantFP::get(*TheContext, APFloat(Val));
}

Value *VariableExprAST::codegen() {
    AllocaInst *A = NamedValues[Name];

    if (!A) {
        LogErrorV("Unknown variable name");
    }
    return Builder->CreateLoad(A->getAllocatedType(), A, Name.c_str());
}

Function *PrototypeAST::codegen() {
    fprintf(stderr, "Prototype Codegen\n");
    std::vector<Type*> Doubles(Args.size(),
                                Type::getDoubleTy(*TheContext));
    FunctionType *FT =
        FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false);

    Function *F =
        Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

    unsigned Idx = 0;
    for (auto &Arg : F->args())
    Arg.setName(Args[Idx++]);

    return F;
}

Value *BinaryExprAST::codegen() {
    fprintf(stderr, "Binary Expr Codegen\n");

    // Special case for assignment operator
    if (Op == '=') {
        VariableExprAST *LHSE = static_cast<VariableExprAST*>(LHS.get());
        if (!LHSE)
            return LogErrorV("Destination of '=' must be a variable");

        Value *Val = RHS->codegen();
        if (!Val)
            return nullptr;
        
        Value *Variable = NamedValues[LHSE->getName()];
        if (!Variable)
            return LogErrorV("Unknown variable name");
        
        Builder->CreateStore(Val, Variable);
        return Val;
    }

    // Regular bin exprs
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
        break;
    }

    Function *F = getFunction(std::string("binary") + Op);
    assert(F && "Binary operator not found!");

    Value *Ops[2] = { L, R };
    return Builder->CreateCall(F, Ops, "binop");
}

Value *CallExprAST::codegen() {
    fprintf(stderr, "Call Expr Codegen\n");

    // Look up the name in the global module table.
    Function *CalleeF = getFunction(Callee);

    if (!CalleeF)
        return LogErrorV("Unknown function referenced");

    // If argument mismatch error.
    if (CalleeF->arg_size() != Args.size())
        return LogErrorV("Incorrect # arguments passed");

    std::vector<Value *> ArgsV;
    // Generates IR to evaluate all arguments first.
    for (unsigned i = 0, e = Args.size(); i != e; ++i) {
        ArgsV.push_back(Args[i]->codegen());
        if (!ArgsV.back())
        return nullptr;
    }

    return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

Function *FunctionAST::codegen() {
    fprintf(stderr, "Function Codegen\n"); 
    
    // First, check for an existing function from a previous 'extern' declaration.
    auto &P = *Proto;
    FunctionProtos[Proto->getName()] = std::move(Proto);
    Function *TheFunction = getFunction(P.getName());
    
    if (!TheFunction)
        return nullptr;

    // operators
    if (P.isBinaryOp())
        BinopPrecedence[P.getOperatorName()] = P.getBinaryPrecedence();

    // Create a new basic block to start insertion into.`
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    // Record the function arguments in the NamedValues map.
    NamedValues.clear();
    for (auto &Arg : TheFunction->args()) {
        AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

        Builder->CreateStore(&Arg, Alloca);

        NamedValues[std::string(Arg.getName())] = Alloca;
    }
    
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
    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);

    // Start
    Value *StartVal = Start->codegen();
    if (!StartVal)
        return nullptr;

    Builder->CreateStore(StartVal, Alloca);
    
    // BB for start of loop body
    BasicBlock *PreheaderBB = Builder->GetInsertBlock();
    BasicBlock *LoopBB =
        BasicBlock::Create(*TheContext, "loop", TheFunction);

    Builder->CreateBr(LoopBB);

    // Start insertion in LoopBB
    Builder->SetInsertPoint(LoopBB);
    PHINode *Variable = Builder->CreatePHI(Type::getDoubleTy(*TheContext),
                                           2, VarName);
    
    Variable->addIncoming(StartVal, PreheaderBB);

    // Value *OldVal = NamedValues[VarName];
    // NamedValues[VarName] = Variable;

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


    // Compute end condition:
    Value *EndCond = End->codegen();
    if (!EndCond)
        return nullptr;

    Value *CurVar = Builder->CreateLoad(Alloca->getAllocatedType(), Alloca,
                                        VarName.c_str());
    Value *NextVar = Builder->CreateFAdd(Variable, StepVal, "nextvar");
    Builder->CreateStore(NextVar, Alloca);
    
    // Exit value of loop
    EndCond = Builder->CreateFCmpONE(
        EndCond, ConstantFP::get(*TheContext, APFloat(0.0)), "loopcond");

    BasicBlock *LoopEndBB = Builder->GetInsertBlock();
    BasicBlock *AfterBB =
        BasicBlock::Create(*TheContext, "afterloop", TheFunction);

    Builder->CreateCondBr(EndCond, LoopBB, AfterBB);

    Builder->SetInsertPoint(AfterBB);

    Variable->addIncoming(NextVar, LoopEndBB);

    // if (OldVal)
    //     NamedValues[VarName] = OldVal;
    // else
    //     NamedValues.erase(VarName);
    
    return Constant::getNullValue(Type::getDoubleTy(*TheContext));
}

Value *UnaryExprAST::codegen() {
  Value *OperandV = Operand->codegen();
  if (!OperandV)
    return nullptr;

  Function *F = getFunction(std::string("unary") + Opcode);
  if (!F)
    return LogErrorV("Unknown unary operator");

  return Builder->CreateCall(F, OperandV, "unop");
}

Value *VarExprAST::codegen() {
    std::vector<AllocaInst *> OldBindings;

    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    // Register all variables, emit initializers.
    const unsigned len = VarNames.size();
    for (unsigned i = 0; i != len; ++i) {
        const std::string &VarName = VarNames[i].first;
        ExprAST *Init = VarNames[i].second.get();

        Value *InitVal;
        if (Init) {
            InitVal = Init->codegen();
            if (!InitVal) 
                return nullptr;
        } else {
            InitVal = ConstantFP::get(*TheContext, APFloat(0.0));
        }

        AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
        Builder->CreateStore(InitVal, Alloca);

        OldBindings.push_back(NamedValues[VarName]);

        NamedValues[VarName] = Alloca;
    }

    // code gen body
    Value *BodyVal = Body->codegen();
    if (!BodyVal)
        return nullptr;
    
    for (unsigned i = 0; i < len; ++i) {
        NamedValues[VarNames[i].first] = OldBindings[i];
    }

    return BodyVal;
}

// Helper function
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