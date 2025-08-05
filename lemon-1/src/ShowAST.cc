#include "../include/AST.h"
#include "../include/Lexer.h"

void LemonAST::showAST() {
    printf("Lemon AST:\n");
    for (auto &statement : statements) {
        statement->showAST();
    }
}

void BinaryExprAST::showAST() {
    printf("BinaryExpr(");
    LHS->showAST();
    if (op == tok_add) printf(" + ");
    if (op == tok_sub) printf(" - ");
    if (op == tok_mul) printf(" * ");
    if (op == tok_div) printf(" / ");
    RHS->showAST();
    printf(")");
}

void NumberExprAST::showAST() {
    printf("Num(%f)", val);
}

void VariableExprAST::showAST() {
    printf("Var(%s)", varName.c_str());
}

void CallExprAST::showAST() {
    printf("CallExpr: %s(", callee.c_str());
    for (auto &item : args) {
        item->showAST();
    }
    printf(")");
}

void PrototypeAST::showAST() {
    printf("Signature: %s(", name.c_str());
    for (auto &item : args) {
        printf("%s, ", item.c_str());
        // item->showAST();
    }
    printf(")\n");
}

void VariableDeclStmt::showAST() {
    printf("Decl: %s = ", varName.c_str());
    defBody->showAST();
    printf(";\n");
}

void AssignmentStmt::showAST() {
    printf("Assign: %s = ", varName.c_str());
    defBody->showAST();
    printf(";\n");
}

void ReturnStmtAST::showAST() {
    printf("return: ");
    retBody->showAST();
    printf(";\n");
}

void FunctionAST::showAST() {
    printf("Function: \n");
    proto->showAST();
    printf("{\n");
    for (auto &statement : functionBody) {
        statement->showAST();
    }
    printf("}\n");
}

void ExternAST::showAST() {
    printf("Extern: ");
    proto->showAST();
    printf("\n");
}

void ExpressionStmtAST::showAST() {
    printf("Expression Statement: ");
    expr->showAST();
    printf("\n");
}