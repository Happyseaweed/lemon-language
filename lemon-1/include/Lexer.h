// ============================================================================
// Lexer
// ============================================================================

#include <string>

#pragma once

enum Token {
    tok_eof = -1,

    tok_func = -2,
    tok_lparen = -3,
    tok_rparen = -4,
    tok_lbrace = -5,
    tok_rbrace = -6,

    tok_comma = -7,
    tok_equal = -8,
    tok_semi = -9,
    tok_return = -10,
    
    tok_add = -11,
    tok_sub = -12,
    tok_mul = -13,
    tok_div = -14,

    tok_var = -15,

    tok_id = -16,
    tok_num = -17,
    
    tok_extern = -18
};

extern std::string idStr;
extern double numVal;
extern int curTok;
extern char curChar;

int gettok();
int getNextToken();