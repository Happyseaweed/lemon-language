#include "../include/AST.h"
#include "../include/Lexer.h"
#include "../include/Parser.h"

// ============================================================================
//                               Error Helpers 
// ============================================================================

std::unique_ptr<ExprAST> LogError(const char *Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
    LogError(Str);
    return nullptr;
}

std::unique_ptr<StmtAST> LogErrorS(const char *Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

Value *LogErrorV(const char *Str) {
  LogError(Str);
  return nullptr;
}

// ============================================================================
//                            Helper Functions
// ============================================================================

int GetTokPrecedence() {
    if (!isascii(CurTok)) 
        return -1;

    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0) 
        return -1;

    return TokPrec;
}

// ============================================================================
//                             Parsing Functions 
// ============================================================================

// Parser
std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto Result = std::make_unique<NumberExprAST>(NumVal);
    getNextToken(); // Consume
    return std::move(Result);
}

std::unique_ptr<ExprAST> ParseParenExpr() {
    getNextToken();
    auto V = ParseExpression(); // Parse inside brackets
    if (!V) 
        return nullptr;

    if (CurTok != ')') 
        return LogError("Expected ')'");

    getNextToken(); 
    return V;
}

std::unique_ptr<ExprAST> ParseIdentifierExpr() {
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


std::unique_ptr<ExprAST> ParseIfExpr() {
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

std::unique_ptr<ExprAST> ParseForExpr() {
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

std::unique_ptr<ExprAST> ParseVarExpr() {
    getNextToken();

    std::vector<std::pair<std::string, std::unique_ptr<ExprAST> > > VarNames;
    
    if (CurTok != tok_identifier)
        return LogError("Expected identifier after var");

    while(true) {
        std::string Name = IdentifierStr;
        getNextToken();

        std::unique_ptr<ExprAST> Init;

        if (CurTok == '=') {
            getNextToken();

            Init = ParseExpression();
            if (!Init) return nullptr;
        }

        VarNames.push_back(std::make_pair(Name, std::move(Init)));

        if (CurTok != ',') break;

        getNextToken();

        if (CurTok != tok_identifier)
            return LogError("Expected identifier list after var");
    }

    if (CurTok != tok_in)
        return LogError("Expected 'in' keyword after var");
    getNextToken();

    auto Body = ParseExpression();
    if (!Body)
        return nullptr;
    
    return std::make_unique<VarExprAST>(std::move(VarNames), std::move(Body));
    
}

std::unique_ptr<ExprAST> ParsePrimary() {
    switch (CurTok) {
        default:
            return nullptr;
            // return LogError("Unknown token when expecting an expression");
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
        case tok_var:
            return ParseVarExpr();
    }
}

std::unique_ptr<ExprAST> ParseUnary() {
    // fprintf(stderr, "Parsing Unary\n");
    if (!isascii(CurTok) || CurTok == '(' || CurTok == ',')
        return ParsePrimary();
    
    // fprintf(stderr, "%d\n", CurTok);
    
    int Opc = CurTok;
    getNextToken();
    if (auto Operand = ParseUnary())
        return std::make_unique<UnaryExprAST>(Opc, std::move(Operand));

    return nullptr;
}

// ============================================================================
//                         Binary Operator Parsing
// ============================================================================

std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS) {
    while (true) {
        int TokPrec = GetTokPrecedence();

        // Interesting
        if (TokPrec < ExprPrec)
            return LHS;

        int BinOp = CurTok;
        getNextToken();

        auto RHS = ParseUnary();
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

std::unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParseUnary();
    if (!LHS) 
        return nullptr;

    return ParseBinOpRHS(0, std::move(LHS));
}

std::unique_ptr<PrototypeAST> ParsePrototype() {
    std::string FnName;

    unsigned Kind = 0; // 0 = identifier, 1 = unary, 2 = binary.
    unsigned BinaryPrecedence = 30;

    switch (CurTok) {
        default:
            return LogErrorP("Expected Function name in prototype");
        case tok_identifier:
            FnName = IdentifierStr;
            Kind = 0;
            getNextToken();
            break;
        case tok_unary:
            getNextToken();
            if (!isascii(CurTok))
                return LogErrorP("Expected unary operator");
            FnName = "unary";
            FnName += (char)CurTok;
            Kind = 1;
            getNextToken();
            break;
        case tok_binary:
            getNextToken();
            if (!isascii(CurTok)) 
                return LogErrorP("Expected binary operator");
            FnName = "binary";
            FnName += (char)CurTok;
            Kind = 2;
            getNextToken();

            if (CurTok == tok_number) {
                if (NumVal < 1 || NumVal > 100)
                    return LogErrorP("Invalid precedence: must be 1...100 ");
                BinaryPrecedence = (unsigned)NumVal;
                getNextToken();
            }
            break;
    }

    if (CurTok != '(')
        return LogErrorP("Expected '(' in prototype");

    std::vector<std::string> ArgNames;
    while (getNextToken() == tok_identifier) {
        ArgNames.push_back(IdentifierStr);
    }
    if (CurTok != ')')
        return LogErrorP("Expected ')' in prototype");

    getNextToken();

    if (Kind && ArgNames.size() != Kind) 
        return LogErrorP("Invalid number of operands for operator.");

    return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames), Kind != 0, BinaryPrecedence);
}

std::unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken();
    auto Proto = ParsePrototype();

    if (!Proto) return nullptr;

    if (auto E = ParseExpression())
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));

    return nullptr;
}

std::unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken();
    return ParsePrototype(); // Function signature
}

// Allow for arbitrary top level expressions and realtime evaluation.
std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if (auto E = ParseExpression()) {
        auto Proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}

std::unique_ptr<StmtAST> ParseStatement() {
    switch (CurTok) {
        case tok_var:
            return ParseVarDecl();
        case tok_lbrace:
            return ParseBlock();
        case tok_identifier:
            return ParseAssignmentOrExprStmt();
        default:
            return ParseExprStatement();
    }
}

std::unique_ptr<StmtAST> ParseVarDecl() {
    // fprintf(stderr, "Parsing Variable Decl\n");
    getNextToken(); // consume 'var'

    if (CurTok != tok_identifier)
        return LogErrorS("Expected Identifier after 'var' ");
   
    std::string VarName = IdentifierStr;
    getNextToken(); // consume ID

    if (CurTok != '=') 
        return LogErrorS("Expected '=' after var name");
    
    getNextToken(); // consume '='

    std::unique_ptr<ExprAST> Init = ParseExpression(); // Get ExprAST for RHS 
    if (!Init) 
        return nullptr; // Bad

    return std::make_unique<VarDeclStmtAST>(VarName, std::move(Init));
}

std::unique_ptr<StmtAST> ParseBlock() {
    return nullptr;
}

//! Change this to just handle assignment stmt?
std::unique_ptr<StmtAST> ParseAssignmentOrExprStmt() {
    std::string VarName = IdentifierStr;
    getNextToken(); // Consume ID;

    // Assignment stmt
    if (CurTok == '=') {
        getNextToken(); // Consume '='
        std::unique_ptr<ExprAST> Expr = ParseExpression();
        
        if (!Expr)
            return nullptr;

        return std::make_unique<AssignmentStmtAST>(VarName, std::move(Expr));   
    }
    return nullptr;
}

//! REMOVE? 
std::unique_ptr<StmtAST> ParseExprStatement() {
    std::unique_ptr<ExprAST> Expr = ParseExpression();
    if (!Expr)
        return nullptr;

    return nullptr;
}

