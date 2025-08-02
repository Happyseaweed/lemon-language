#include "../include/Lexer.h"

std::string idStr;
double numVal;
int curTok;
char curChar = ' ';

int gettok() {
    while(isspace(curChar)) curChar = getchar();


    // Alpha-numeric identifiers (keywords or IDs)
    if (isalpha(curChar)) {
        idStr = curChar;

        while(isalnum(curChar = getchar())) {
            idStr += curChar;
        }

        // Check for keywords
        if (idStr == "func")    
            return tok_func;
        if (idStr == "var")
            return tok_var;
        if (idStr == "extern")
            return tok_extern;
        if (idStr == "return")
            return tok_return;
        
        // Not keyword
        return tok_id; 
    }

    // Numerical values, standard parsing, doesn't check for xx.xx.xx
    // Doesn't check for multiple decimal points
    if (isdigit(curChar) || curChar == '.') {
        std::string numStr;
        do {
            numStr += curChar;
            curChar = getchar();
        } while (isdigit(curChar) || curChar == '.');

        numVal = strtod(numStr.c_str(), 0);
        return tok_num;
    }

    // Comments
	if (curChar == '#') {
		do 
			curChar = getchar();
        while(curChar != EOF && curChar != '\n' && curChar != '\r');

        if (curChar != EOF)
            return gettok();
	}

    // Brace, parens 
    if (curChar == '{') {
        curChar = getchar();
        return tok_lbrace;
    }
    if (curChar == '}') {
        curChar = getchar();
        return tok_rbrace;
    }
    if (curChar == '(') {
        curChar = getchar();
        return tok_lparen;
    }
    if (curChar == ')') {
        curChar = getchar();
        return tok_rparen;
    }
    
    // Binary OPs
    if (curChar == '+') {
        curChar = getchar();
        return tok_add;
    }
    if (curChar == '-') {
        curChar = getchar();
        return tok_sub;
    }
    if (curChar == '*') {
        curChar = getchar();
        return tok_mul;
    }
    if (curChar == '/') {
        curChar = getchar();
        return tok_div;
    }

    // Special symbols
    if (curChar == ';') {
        curChar = getchar();
        return tok_semi;
    } 
    if (curChar == ',') {
        curChar = getchar();
        return tok_comma;
    }
    if (curChar == '=') {
        curChar = getchar();
        return tok_equal;
    }

    // EOF
    if (curChar == EOF)
        return tok_eof;
    
    // Anything else not supported.
    int unsupportedChar = curChar;
    curChar = getchar();
    return unsupportedChar;
}

int getNextToken() {
    return curTok = gettok();
}