#include "../include/Parser.h"
#include "../include/AST.h"
#include "../include/Lexer.h"

// ============================================================================
//          Mock "library" functions to be "extern'd" in user code
// ============================================================================

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}

// ============================================================================
//                                  Main
// ============================================================================

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