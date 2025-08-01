# Basic grammar for Lemon


MAIN 
    := STMT_LIST 

STMT_LIST
    := STMT | STMT STMT_LIST 

STMT
    := IF_STMT 
    := FOR_STMT 
    := WHILE_STMT
    := DECL_STMT 
    := ASSIGN_STMT 

IF_STMT
    := 'if' '(' EXPR ')' '{' STMT_LIST'}' 'else' '{' STMT_LIST '}'

FOR_STMT
    := 'for' ITER 'in' '(' FOR_BEGIN, FOR_END ')' '{' STMT_LIST '}'

DECL_STMT
    := 'var' ID '=' EXPR

ASSIGN_STMT
    := ID '=' EXPR 

EXPR
