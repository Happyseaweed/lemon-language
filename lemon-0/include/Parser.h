// Error Functions
std::unique_ptr<ExprAST> LogError(const char *Str);

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);

std::unique_ptr<StmtAST> LogErrorS(const char *Str);

Value *LogErrorV(const char *Str);

// Helper Functions
int GetTokPrecedence();

// Parsing Functions
std::unique_ptr<ExprAST> ParseExpression();

std::unique_ptr<ExprAST> ParseParenExpr();

std::unique_ptr<ExprAST> ParseIdentifierExpr();

std::unique_ptr<ExprAST> ParseIfExpr();

std::unique_ptr<ExprAST> ParseForExpr();

std::unique_ptr<ExprAST> ParseVarExpr();

std::unique_ptr<ExprAST> ParsePrimary();

std::unique_ptr<ExprAST> ParseUnary();

std::unique_ptr<ExprAST> ParseBinOpRHS();

std::unique_ptr<ExprAST> ParseExpression();

std::unique_ptr<PrototypeAST> ParsePrototype();

std::unique_ptr<FunctionAST> ParseDefinition();

std::unique_ptr<PrototypeAST> ParseExtern();

std::unique_ptr<FunctionAST> ParseTopLevelExpr();

// Statements:
std::unique_ptr<StmtAST> ParseVarDecl();

std::unique_ptr<StmtAST> ParseBlock();

std::unique_ptr<StmtAST> ParseAssignmentOrExprStmt();

std::unique_ptr<StmtAST> ParseExprStatement();

std::unique_ptr<StmtAST> ParseStatement();
