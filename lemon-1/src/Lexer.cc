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
        if (idStr == "if")
            return tok_if;
        if (idStr == "else")
            return tok_else;

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

    // Comparison ops, need to peak next char.
    if (curChar == '<') {
        char peakChar = getchar();
        if (peakChar == '=') {
            curChar = getchar();
            return tok_le;
        }
        curChar = peakChar;
        return tok_lt;
    }
    if (curChar == '>') {
        char peakChar = getchar();
        if (peakChar == '=') {
            curChar = getchar();
            return tok_ge;
        }
        curChar = peakChar;
        return tok_gt;
    }
    if (curChar == '=') {
        char peakChar = getchar();
        if (peakChar == '=') {
            curChar = getchar();
            return tok_eq;
        }
        curChar = peakChar;
        return tok_assign;
    }
    if (curChar == '!') {
        char peakChar = getchar();
        if (peakChar == '=') {
            curChar = getchar();
            return tok_neq;
        }
        // put it back because now '!' is undefined op (for now...)
        ungetc(peakChar, stdin);
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

int peakNextToken() {
    int nextTok = gettok(); // Gets token starting at curChar
    std::string nextTokStr = tokenToString(nextTok);

    // Put string back to input stream.
    if (nextTok != tok_eof)  {
        // put white space back first
        // Put each character one by one back to input stream.
        // Reverse order
        // Put back curChar first.
        if (ungetc(curChar, stdin) == EOF) {
            fprintf(stderr, "🍋 (%c) Failed to ungetc curChar.", curChar);
        }
        for (auto it = nextTokStr.rbegin(); it != nextTokStr.rend(); ++it) {
            if (ungetc(*it, stdin) == EOF) {
                fprintf(stderr, "🍋 (%c) Failed to ungetc token char.", *it);
            }
        }
        curChar = ' '; // reset curChar so parsing doesn't kill itself...
    }
    return nextTok;
}

std::string tokenToString(int token) {
    switch (token) {
    case tok_eof:
        return "";
    case tok_func:
        return "func";
    case tok_lparen:
        return "(";
    case tok_rparen:
        return ")";
    case tok_lbrace:
        return "{";
    case tok_rbrace:
        return "}";
    case tok_comma:
        return ",";
    case tok_assign:
        return "=";
    case tok_semi:
        return ";";
    case tok_return:
        return "return";
    case tok_add:
        return "+";
    case tok_sub:
        return "-";
    case tok_mul:
        return "*";
    case tok_div:
        return "/";
    case tok_var:
        return "var";
    case tok_id:
        return idStr;
    case tok_num:
        return std::to_string(numVal);
    case tok_extern:
        return "extern";
    case tok_if:
        return "if";
    case tok_else:
        return "else";
    case tok_lt:
        return "<";
    case tok_gt:
        return ">";
    case tok_le:
        return "<=";
    case tok_ge:
        return ">=";
    case tok_eq:
        return "==";
    case tok_neq:
        return "!=";
    default:
        return "Unknown Token";
    }
}
