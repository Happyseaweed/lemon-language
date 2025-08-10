
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

---
# Compilation Details:
### REPL Mode:
Each block is parsed as it comes in.
Then LLVM IR is generated, and then it is optimized, and executed.
All side effects should happen, and codegen would only need 1 main builder 
that builds top to bottom.

For global variables, they can be defined, and then immediately evaluated
and their inits are stored.

1. Parse lemon program line by line, as they are inputted.
2. Generate LLVM IR for the current block:
    - Global var decls: add global var, then initialize it.
3. Add to JIT, evaluate

### AOT mode:
1. Parse lemon program line by line
2. Generate LLVM IR:
    - Global variables: If init expr is constant, init directly.
    - Global variables: non constant init expr, create init function and call it in lemon_main()
3. Return LLVM IR for compilation to machine code.


---
# Refactoring codegen() and better builder handling.
1. Extern statements for imports are added globally.
2. Func statements have their own scopes, built with regular Builder.
3. Anything else is put in lemon_main() and are executed sequentially.
4. Generation occurs as the file is being read, meaning declaration before
   use is enforced for both variables and functions.


---
# How IRBuilder<> Works
Builder acts as an iterator for where to insert new LLVM IR.

We keep track of 2 builders:
1. MainBuilder: inserts IR into the lemon_main function block
2. Builder: inserts IR for anything outside of lemon_main (Other funcs)

##### Rules:
1. Any statement or expression-statement in global space is inserted into main.
2. Any global var decl also have initializer functions that are called in main.
3. Any function decls and their contents are handled by builder.