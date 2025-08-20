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

std::unique_ptr<ExprAST> LogError(const char *str) {
    fprintf(stderr, "ERROR: %s\n", str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *str) {
    LogError(str);
    return nullptr;
}

std::unique_ptr<StmtAST> LogErrorS(const char *str) {
    fprintf(stderr, "ERROR: %s\n", str);
    return nullptr;
}

std::unique_ptr<FunctionAST> LogErrorF(const char *str) {
    fprintf(stderr, "ERROR: %s\n", str);
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
        if (curTok == tok_eof || curTok == tok_rbrace) 
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
        // case tok_var:
        case tok_double:
        case tok_tensor:
            return ParseVariableDecl();
        case tok_id:
            return ParseVariableAssignOrFunctionCall();
        case tok_if:
            return ParseIfStmt();
        case tok_func:
            return ParseFunction();
        case tok_extern:
            return ParseExtern();
        case tok_for:
            return ParseForStmt();
        
        default:
            return nullptr;
            // fprintf(stderr, "ERROR: Token '%d' is not defined.\n", curTok);
            // return LogErrorS("Unknown token when parsing statement.");
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
    getNextToken();
    
    return std::make_unique<ReturnStmtAST>(std::move(E));
}

std::unique_ptr<StmtAST> ParseVariableDecl() {
    // var ID = EXPR;
    // Does not allow chaining (yet): var ID1, ID1, ID3, = EXPR1, EXPR2, EXPR3;

    std::string varName;
    bool isDouble = curTok == tok_double ? true : false;

    getNextToken(); // Consume variable type keyword

    varName = idStr;
    getNextToken(); // Consume ID

    if (curTok != tok_assign)
        return LogErrorS("Expected '=' in variable declaration statement.");
    getNextToken(); // Consume '='
        
    auto E = ParseExpression();
    if (!E)
        return nullptr;

    if (curTok != tok_semi)
        return LogErrorS("Expected ';' after statement.");
    getNextToken();
    
    if (isDouble)
        E->type.kind = LemonType::TypeKind::Double;
    else 
        E->type.kind = LemonType::TypeKind::Tensor;

    return std::make_unique<VariableDeclStmt>(varName, std::move(E));
}

std::unique_ptr<StmtAST> ParseVariableAssignOrFunctionCall() {
    // Parses statements that start with IDs.
    //      - Variable assignments  e.g. x = 10;
    //      - Tensor assignments    e.g. x[idx] = 10;
    //      - Function calls        e.g. x(argList);

    int peakedToken = peakNextToken();
    // printf("Parsing variable decl or function call \n");

    if (peakedToken == tok_assign || peakedToken == tok_lbracket) {
        return ParseVariableAssign();
    }
    else if (peakedToken == tok_lparen) {
        auto expr = ParseIdentifierExpr(); // Should return parsed function call
        
        if (curTok != tok_semi)
            return LogErrorS("Expected ';' after expression statement.");
        getNextToken(); // Consume ';'

        return std::make_unique<ExpressionStmtAST>(std::move(expr));
    }
    
    return nullptr; 
}

std::unique_ptr<StmtAST> ParseVariableAssign() {
    // ID   = EXPR;
    // ID[] = EXPR;
    // Does not allow chaining.
    std::string varName = idStr;
    printf("Variable assign: (%s)\n", varName.c_str());
    std::vector<std::unique_ptr<ExprAST>> indexExpressions;
    getNextToken(); // consume ID

    // Parse brackets
    if (curTok == tok_lbracket) {

        while(true) {
            if (curTok != tok_lbracket) {
                return LogErrorS("Expected open bracket '[' in subscripts.");
            }
            getNextToken(); // consume '['

            auto E = ParseExpression();

            if (!E) {
                return LogErrorS("Expected expression as index in [].");
            }
            
            indexExpressions.push_back(std::move(E));
            
            if (curTok != tok_rbracket) {
                return LogErrorS("Expected close bracket ']' in subscripts.");
            }
            getNextToken();
            
            if (curTok == tok_assign) {
                break;
            }
        }
    }

    if (curTok != tok_assign)
        return LogErrorS("Expected '=' in variable assignment statement.");
    getNextToken();

    auto E = ParseExpression();
    if (!E)
        return nullptr;

    if (curTok != tok_semi)
        return LogErrorS("Expected ';' after statement.");
    getNextToken();
    
    return std::make_unique<AssignmentStmt>(varName, std::move(indexExpressions), std::move(E));
}

// ============================================================================
// Function and Function signature (Prototype)
// ============================================================================

std::unique_ptr<FunctionAST> ParseFunction() {
    // func ID ( arg_list ) { STATEMENT LIST 
    getNextToken(); // Consumes 'func' keyword
    
    auto proto = ParsePrototype();

    if (!proto)
        return nullptr;

    // Now we only have { STATEMENT LIST } left to parse.

    if (curTok != tok_lbrace)
        return LogErrorF("Expected block '{' after function signature.");
    getNextToken(); // Consume '{'

    auto stmtList = ParseStatementList(); // Might need to change to support returns.

    if (curTok != tok_rbrace)
        return LogErrorF("Expected closing '}' after function body.");
    getNextToken(); // Consume '}'


    return std::make_unique<FunctionAST>(std::move(proto), std::move(stmtList));
}

std::unique_ptr<PrototypeAST> ParsePrototype() {
    // func ID ( arg_list ) 
    // Only consumes the above. Does not support forward declaration (yet)
    std::string fnName;
    std::vector<std::string> argList;

    if (curTok != tok_id) 
        return LogErrorP("Function signature expected identifier.");
    fnName = idStr;
    getNextToken(); // Consume ID
    
    if (curTok != tok_lparen)
        return LogErrorP("Expected '(' in function signature.");
    getNextToken(); // Consume '('

    // Arg list
    if (curTok != tok_rparen) {
        while(true) {
            // Args must be IDs or calls.
            //! NOTE: Be careful for externs. Not allowed to have function call expr in arg list.

            if (curTok != tok_id) 
                return LogErrorP("Expected ID or ID() in function signature argument list.");

            argList.push_back(idStr);
            getNextToken();                

            if (curTok == tok_rparen)
                break;
            
            if (curTok != tok_comma) {
                return LogErrorP("Expected ')' or ',' in function signature argument list.");
            }
            
            getNextToken();
        }
    }
    getNextToken(); // Consumes ')'

    return std::make_unique<PrototypeAST>(fnName, std::move(argList));    
}


std::unique_ptr<StmtAST> ParseExtern() {
    getNextToken(); // Consume 'extern' keyword

    auto proto = ParsePrototype();

    if (curTok != tok_semi) 
        return LogErrorS("Expected ';' after extern definition.");
    getNextToken(); // Consume ';'
    
    return std::make_unique<ExternAST>(std::move(proto));
}

std::unique_ptr<StmtAST> ParseIfStmt() {
    getNextToken(); // Consume 'if'
        
    if (curTok != tok_lparen)
        return LogErrorS("Expected '(' after 'if' keyword.");
    getNextToken(); // Consume '('

    auto cond = ParseExpression();
    if (!cond)
        return LogErrorS("Expected expression after 'if'.");
    
    if (curTok != tok_rparen)
        return LogErrorS("Expected ')' after 'if' condition.");
    getNextToken(); // Consume ')'

    if (curTok != tok_lbrace)
        return LogErrorS("Expected '{' after 'if' condition.");
    getNextToken(); // Consume '{'

    auto thenBody = ParseStatementList();
    if (curTok != tok_rbrace)
        return LogErrorS("Expected '}' after 'if' body.");
    getNextToken(); // Consume '}'
    
    // If no else statement, return if stmtAST with empty body
    if (curTok != tok_else) 
        return std::make_unique<IfStmtAST>(std::move(cond), std::move(thenBody), std::vector<std::unique_ptr<StmtAST>>());

    getNextToken(); // Consume 'else'

    if (curTok != tok_lbrace)
        return LogErrorS("Expected '{' after 'else' keyword.");
    getNextToken(); // Consume '{'

    auto elseBody = ParseStatementList();
    if (curTok != tok_rbrace)
        return LogErrorS("Expected '}' after 'else' body.");
    getNextToken(); // Consume '}'

    return std::make_unique<IfStmtAST>(std::move(cond), std::move(thenBody), std::move(elseBody));
}

std::unique_ptr<StmtAST> ParseForStmt() {
    // for (start, end, step) { stmt_list }
    std::string iterator = "";
    getNextToken(); // Consume 'for';

    if (curTok != tok_lparen)
        return LogErrorS("Expected '(' in for loop definition");
    getNextToken(); // consume '('

    if (curTok != tok_id)
        return LogErrorS("Expected iterator ID in for loop definition.");
    iterator = idStr;
    getNextToken(); // Consume ID

    if (curTok != tok_assign)
        return LogErrorS("Expected '=' in for loop start definition.");
    getNextToken(); // consume '=';
    
    auto start = ParseExpression();
    if (!start)
        return nullptr;
    
    if (curTok != tok_comma)    
        return LogErrorS("Expected separator ',' after for loop start definition.");
    getNextToken(); // Consume ','

    auto end = ParseExpression(); 
    if (!end)
        return nullptr;
    
    // Optional step value, default is 1.0
    std::unique_ptr<ExprAST> step;
    if (curTok == tok_comma) {
        getNextToken(); // consume ','
        step = ParseExpression();
        if (!step)
            return nullptr;
    } else {
        step = std::make_unique<NumberExprAST>(1.0);
    }

    if (curTok != tok_rparen) 
        return LogErrorS("Expected ')' after for loop definition.");
    getNextToken(); // consume ')';

    if (curTok != tok_lbrace)
        return LogErrorS("Expected '{' in for loop body definition.");
    getNextToken();

    auto forBody = ParseStatementList();

    if (curTok != tok_rbrace)
        return LogErrorS("Expected '}' closing brace in for loop body definition.");
    getNextToken();
        
    return std::make_unique<ForStmtAST>(iterator, std::move(start), std::move(end), std::move(step), std::move(forBody));
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
        if (opAfterRHSPrecedence > nextOpPrecedence) {      // Decide: recurse
            RHS = ParseBinOpRHS(nextOpPrecedence+1, std::move(RHS)); // +1 to prevent infinite (???)
            if (!RHS) 
                return nullptr;
        }

        LHS = std::make_unique<BinaryExprAST>(binOP, std::move(LHS), std::move(RHS));
    }
}

std::unique_ptr<ExprAST> ParseFactor() {
    // printf("Parsing Factor: curTok: %d\n", curTok);

    if (curTok == tok_id) {
        return ParseIdentifierExpr();       // ID or Func call.
    }
    else if (curTok == tok_num) {
        return ParseNumberExpr();           // Number
    }
    else if (curTok == tok_lbracket) {
        return ParseTensorExpr();           // Tensor element
    }
    else if (curTok == tok_lparen) {        
        getNextToken(); // consume '('
        auto E = ParseExpression();         // '(' Expression ')'

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

    result->type.kind = LemonType::TypeKind::Double;

    return std::move(result);
}

std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    std::string identifier = idStr;                     // Id name
    std::vector<std::unique_ptr<ExprAST>> subscripts;   // For tensor subscript expr
    std::vector<std::unique_ptr<ExprAST>> argList;      // For function call expr
    getNextToken(); // Consume ID;

    // If just an ID
    if (curTok != tok_lparen && curTok != tok_lbracket) {
        return std::make_unique<VariableExprAST>(identifier);
    }

    // If subscripts
    if (curTok == tok_lbracket) {
        getNextToken(); // Consume '['

        while(true) {
            auto E = ParseExpression();
            if (!E) {
                return LogError("Expected expression in subscript operator.");
            }

            subscripts.push_back(std::move(E));
            if (curTok != tok_rbracket) {
                return LogError("Expected ']' after subscript.");
            }
            getNextToken(); //Consumes ']'
            
            if (curTok != tok_lbracket) {
                break;
            }
            getNextToken(); // Consumes next subscript's '[' char.
        }

        printf("Parsed a subscript AST for tensor: %s\n", identifier.c_str());
        return std::make_unique<SubscriptExprAST>(identifier, std::move(subscripts));
    }

    // If function call
    getNextToken(); // consume '('

    // Arg list
    if (curTok != tok_rparen) {
        while(true) {
            auto arg = ParseExpression();
            if (!arg) {
                return nullptr;
            }

            argList.push_back(std::move(arg));

            if (curTok == tok_rparen)
                break;
            
            if (curTok != tok_comma) {
                return LogError("Expected ')' or ',' in argument list.");
            }
            
            getNextToken();
        }
    }
    
    getNextToken(); // Consume ')'
    
    return std::make_unique<CallExprAST>(identifier, std::move(argList));
}

std::unique_ptr<ExprAST> ParseTensorExpr() {
    // [...]
    std::vector<size_t> shape;
    std::vector<std::unique_ptr<ExprAST>> values;
    size_t curDimensionCount = 0;
    std::vector<size_t> subShape;
    int subTensorCount = 0;

    getNextToken(); // Consume '['

    while(true) {
        if (curTok == tok_lbracket) {
            // sub-tensor
            printf("Tensor in Tensor\n");
            auto E = ParseTensorExpr();
            if (!E) {
                return nullptr;
            }
            
            subTensorCount += 1;
            TensorExprAST* TE = dynamic_cast<TensorExprAST*>(E.get());

            if (subShape.size() > 0 && subShape != TE->getShape()) {
                return LogError("Tensor shape incorrect, different shaped sub-tensors.");               
            } else {
                subShape = TE->getShape();
            }
            
            values.push_back(std::move(E));
        }
        else if (curTok == tok_num) {
            auto E = ParseNumberExpr();
            printf("Num in Tensor\n");
            if (!E) {
                return nullptr;
            }

            if (subTensorCount > 0) {
                return LogError("Tensor shape incorrect, cannot contain mix of tensors and nums.");
            }

            values.push_back(std::move(E));
        }
        else if (curTok == tok_id) {
            auto E = ParseIdentifierExpr();
            printf("Num in Tensor\n");
            if (!E) {
                return nullptr;
            }
            
            if (subTensorCount > 0) {
                return LogError("Tensor shape incorrect, cannot contain mix of tensors and IDs.");
            }

            values.push_back(std::move(E));
        }
        else {
            return LogError("Expected expression in tensor decl list.");
        }
        curDimensionCount++;

        if (curTok == tok_rbracket) {
            break;
        }

        if (curTok != tok_comma) {
            printf("CurTok: %d\n", curTok);
            return LogError("Expected ',' in tensor decl list.");
        }
        getNextToken(); // Consume ','
    }

    if (curTok != tok_rbracket) {
        return LogError("Expected ']' after tensor decl.");
    }
    getNextToken(); // consume ']'
    
    // Checking for bad shapes
    if (subTensorCount > 0 && subTensorCount != curDimensionCount) {
        return LogError("Tensor shape incorrect, cannot contain mix of tensors and NUM/IDs.");
    }   

    // Update shape for current tensor
    shape.push_back(curDimensionCount);
    if (subShape.size() != 0) {
        for (auto &item : subShape) {
            shape.push_back(item);
        }
    }
    
    std::unique_ptr<TensorExprAST> tensor = 
        std::make_unique<TensorExprAST>(shape, std::move(values));

    printf("It is a tensor!\n");
    tensor->type.kind = LemonType::TypeKind::Tensor;
    tensor->type.shape = shape;
    
    return std::move(tensor);
}
