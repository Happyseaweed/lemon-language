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
    tok_assign = -8,
    tok_semi = -9,
    tok_return = -10,
    
    tok_add = -11,
    tok_sub = -12,
    tok_mul = -13,
    tok_div = -14,

    // tok_var = -15,

    tok_id = -16,
    tok_num = -17,
    
    tok_extern = -18,

    tok_if = -19,
    tok_else = -20,

    tok_lt = -21,
    tok_gt = -22,
    tok_le = -23,
    tok_ge = -24,
    tok_eq = -25,
    tok_neq = -26,

    tok_for = -27,

    tok_lbracket = -28,
    tok_rbracket = -29,

    tok_double = -30,
    tok_tensor = -31
};

extern std::string idStr;
extern double numVal;
extern int curTok;
extern char curChar;

int gettok();
int getNextToken();
int peakNextToken();
std::string tokenToString(int token);
