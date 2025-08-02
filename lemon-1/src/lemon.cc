#include "../include/Parser.h"
#include "../include/AST.h"
#include "../include/Lexer.h"


void runLemon() {
    getNextToken();
    while (true) {
        switch (curTok) {

        case tok_eof:
            return;
        
        case tok_func:
            //?
            break;
        
        case tok_extern:
            //?
            break;

        default:
            auto result = Parse();
            printf("Parsed\n");
            result->showAST();
            break;
        }
    }
}

int main() {
    operatorPrecedence[tok_add] = 20;
    operatorPrecedence[tok_sub] = 30;
    operatorPrecedence[tok_mul] = 40;
    operatorPrecedence[tok_div] = 40;

    runLemon();

    return 0;
}