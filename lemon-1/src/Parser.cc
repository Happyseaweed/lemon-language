#include "../include/Parser.h"
#include "../include/Lexer.h"


// ============================================================================
//                                Variables
// ============================================================================
std::map<int, int> operatorPrecedence;


int getPrecedence(int tok) {
    // All precedence is > 0
    int precedence = operatorPrecedence[tok];

    if (precedence <= 0)
        return -1;

    return precedence;
}


// ============================================================================
//                               Error Helpers 
// ============================================================================

std::unique_ptr<ExprAST> LogError(const char *Str) {
    fprintf(stderr, "ERROR: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
    LogError(Str);
    return nullptr;
}

std::unique_ptr<StmtAST> LogErrorS(const char *Str) {
    fprintf(stderr, "ERROR: %s\n", Str);
    return nullptr;
}

Value *LogErrorV(const char *Str) {
  LogError(Str);
  return nullptr;
}

// ============================================================================
//                              Parsing Functions 
// ============================================================================

std::unique_ptr<LemonAST> Parse() {
    auto stmtList = ParseStatementList();

    return std::make_unique<LemonAST>(std::move(stmtList), 0);
}

std::vector<std::unique_ptr<StmtAST>> ParseStatementList() {
    std::vector<std::unique_ptr<StmtAST>> stmtList;
    
    while(true) {
        if (curTok == tok_eof) 
            break;

        stmtList.push_back(ParseStatement());
    }

    return stmtList;
}

std::unique_ptr<StmtAST> ParseStatement() {
    // Handles return, decl, assign. 
    //     - Functions defs and externs are handled at higher level (?)

    // Future: if/else, for, while.

    switch (curTok) {
        case tok_return:
            return ParseReturn();
        case tok_var:
            return ParseVariableDecl();
        case tok_id:
            return ParseVariableAssign();
        
        default:
            fprintf(stderr, "ERROR: Token '%d' is not defined.", curTok);
            return LogErrorS("Unknow token when parsing statement.");
    }
}

std::unique_ptr<StmtAST> ParseReturn() {
    // return EXPR;
    getNextToken(); // Consume 'return' keyword

    auto E = ParseExpression();

    if (!E)
        return LogErrorS("Expected expression in return statement.");

    if (curTok != tok_semi)
        return LogErrorS("Expected ';' after return statement.");
    
    return std::make_unique<ReturnStmtAST>(std::move(E));
}

std::unique_ptr<StmtAST> ParseVariableDecl() {
    // var ID = EXPR;
    // Does not allow chaining (yet): var ID1, ID1, ID3, = EXPR1, EXPR2, EXPR3;

    std::string varName;
    getNextToken(); // Consume 'var' kw

    varName = idStr;
    getNextToken(); // Consume ID

    if (curTok != tok_equal)
        return LogErrorS("Expected '=' in variable declaration statement.");
    getNextToken(); // Consume '='
        
    auto E = ParseExpression();
    if (!E)
        return nullptr;

    if (curTok != tok_semi)
        return LogErrorS("Expected ';' after statement.");
    getNextToken();
    
    return std::make_unique<VariableDeclStmt>(varName, std::move(E));
}

std::unique_ptr<StmtAST> ParseVariableAssign() {
    // ID = EXPR;
    // Does not allow chaining.
    std::string varName = idStr;
    getNextToken();

    if (curTok != tok_equal)
        return LogErrorS("Expected '=' in variable assignment statement.");
    getNextToken();

    auto E = ParseExpression();
    if (!E)
        return nullptr;

    if (curTok != tok_semi)
        return LogErrorS("Expected ';' after statement.");
    getNextToken();
    
    return std::make_unique<AssignmentStmt>(varName, std::move(E));
}

// ============================================================================
// Expression parsing (Precedence climbing)
// ============================================================================

std::unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParseFactor();

    if (!LHS)
        return nullptr;

    return ParseBinOpRHS(0, std::move(LHS));
}

std::unique_ptr<ExprAST> ParseBinOpRHS(int precedence, std::unique_ptr<ExprAST> LHS) {
    // a+b*c
    // ^ Start with a as LHS and prec = 0.
    // Find '+' and compare '+' precedence against 0 first.
    // If works, then parse b and peak next binOP '*'
    // If next binOP '*' has higher precedence than '+', recurse.
    // Otherwise, group (a+b) first.

    while (true) {
        // Check next operator.
        int nextOpPrecedence = getPrecedence(curTok);
        printf("IdStr: %s\n", idStr.c_str());
        printf("current prec: %d\n", precedence);
        printf("next op precedence: %d\n", nextOpPrecedence);

        // If next character is binOP but less significant, don't include in current binop
        // Or if next character is not binOP, getPrecedence() should return -1.
        if (nextOpPrecedence < precedence)   
            return LHS; 

        // RHS is binOP, and precedence is at least current.
        int binOP = curTok;
        getNextToken();  // Consume next binOP.

        auto RHS = ParseFactor(); // Parse what ever is left on right side (FACTOR only)
        if (!RHS)
            return nullptr;

        int opAfterRHSPrecedence = getPrecedence(curTok);
        printf("after RHS prec: %d\n", opAfterRHSPrecedence);
        if (opAfterRHSPrecedence > nextOpPrecedence) {      // Decide: recurse
            RHS = ParseBinOpRHS(nextOpPrecedence+1, std::move(RHS)); // +1 to prevent infinite (???)
            printf("test\n");
            if (!RHS) 
                return nullptr;
        }

        LHS = std::make_unique<BinaryExprAST>(binOP, std::move(LHS), std::move(RHS));
    }
}

std::unique_ptr<ExprAST> ParseFactor() {
    if (curTok == tok_id) {
        return ParseIdentifierExpr();       // ID or Func call.
    }
    else if (curTok == tok_num) {
        return ParseNumberExpr();           // Number
    }
    else if (curTok == tok_lparen) {        // '(' Expression ')'
        getNextToken(); // consume '('
        auto E = ParseExpression();

        if (curTok != tok_rparen) 
            return LogError("Expected ')' after expression.");
        getNextToken(); // Consume ')';
        
        return std::move(E);
    }
    return nullptr;
}


std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto result = std::make_unique<NumberExprAST>(numVal);
    getNextToken(); // Consume num token
    return std::move(result);
}

std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    std::string identifier = idStr;
    getNextToken(); // Consume ID;

    // If just an ID
    if (curTok != tok_lparen)
        return std::make_unique<VariableExprAST>(identifier);

    // If function call
    std::vector<std::unique_ptr<ExprAST>> argList;
    getNextToken(); // consume '('

    // Arg list
    if (curTok != tok_rparen) {
        while(true) {
            if (auto arg = ParseExpression()) {
                argList.push_back(std::move(arg));
            } else {
                return nullptr;
            }

            if (curTok == tok_rparen)
                break;
            
            if (curTok != tok_comma) {
                return LogError("Expected ')' or ',' in argument list.");
            }
            
            getNextToken();
        }
    }
    
    getNextToken(); // Consume ')'

    if (curTok != tok_semi)
        return LogError("Expected ';' after function call.");
    
    return std::make_unique<CallExprAST>(identifier, std::move(argList));
}

// ============================================================================
// Function and Function signature (Prototype)
// ============================================================================

std::unique_ptr<FunctionAST> ParseFunction() {
    getNextToken(); // Consumes 'func'
    auto proto = ParsePrototype();

    if (!proto) {
        fprintf(stderr, "ERROR: Function prototype parsing failed.\n");
        return nullptr;
    }

    // Optional block { }, block contains list, otherwise just 1 statement.

    auto sBody = ParseStatementList();

    return std::make_unique<FunctionAST>(std::move(proto), std::move(sBody));
}

std::unique_ptr<PrototypeAST> ParsePrototype() {
    // func ID ( ARG_LIST ) { STMT_LIST }
    std::vector<std::string> args;
    std::string functionName;

    if (curTok != tok_id) {
        return LogErrorP("Expected function identifier in prototype.");
    }
    functionName = idStr;

    getNextToken(); // Consume ID

    if (curTok != tok_lparen)
        return LogErrorP("Expected '(' in protype.");

    getNextToken(); // Consume '(', now curTok should be ID or ')'
    while(curTok != tok_rparen) {
        if (curTok == tok_id) 
            args.push_back(idStr);
        getNextToken();

        if (curTok != tok_comma) 
            return LogErrorP("Expected ',' in list of arguments for Prototype."); 
        getNextToken(); 
    }

    // Sanity checking:
    fprintf(stderr, "Parsed prototype args list: ");
    for (auto &item : args) {
        fprintf(stderr, "%s ", item.c_str());
    }
    // End of sanity checking

    return std::make_unique<PrototypeAST>(functionName, std::move(args));  
}

