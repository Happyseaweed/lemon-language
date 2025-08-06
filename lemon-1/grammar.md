
# Grammar For Lemon-1 
Imperative, Statically Typed

LEMON           ::= STATEMENT_LIST

STATEMENT_LIST  ::= STATEMENT
                  | STATEMENT STATEMENT_LIST

STATEMENT       ::= FUNCTION_DECL_STMT
                  | VARIABLE_DECL_STMT
                  | ASSIGNMENT_STMT
                  | RETURN_STMT

FUNCTION_DECL_STMT  ::= 'func' ID '(' ARG_LIST ')' '{' STATEMENT_LIST '}'

ARG_LIST            ::= .NONE
                      | ID
                      | ID ',' ARG_LIST

VARIABLE_DECL_STMT  ::= 'var' ID '=' EXPRESSION ';'

ASSIGNMENT_STMT     ::= ID '=' EXPRESSION ';'

RETURN_STMT         ::= 'return' EXPRESSION ';'

EXPRESSION          ::= TERM
                      | EXPRESSION '+' TERM 
                      | EXPRESSION '-' TERM

TERM                ::= FACTOR
                      | TERM '*' FACTOR
                      | TERM '/' FACTOR

FACTOR              ::= ID
                      | NUM
                      | ID '(' ')'
                      | '(' EXPRESSION ')'



---

# Sample program:
```
extern printd(x);

var x = 10;

func test() {
    var y = x + 10;
    printd(y);
}

test();
```

---

# Control Flows:
### if/else statement: 
```
if (expr) {
    stmtList;
} else {
    stmtList;
}
```

### for loops:
```
for (i = 0, i < n, 1.0) {
    stmtList;
}
```

### while loops:
```
while (expr) {
    stmtList;
}
```
