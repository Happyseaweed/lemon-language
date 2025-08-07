# lemon-language
Lemon programming language

Lemon is a simple programming language built around tensor ops and leverages LLVM to optimize and compile.

The goal is: 
1) Systematic exploration of LLVM, IR generation and optimization passes.
2) Create a simple language that is capable of implementing simple ML concepts.

Features:
- Types: float, tensor
- Conditionals: if/else, for
- Functions

# Building
```
mkdir build
cd build
cmake ../
make
```

# Running
I added an alias to zsh rc that directly triggers the executable.
```
alias lemon="<path-to-executable>/lemon <"
```
So I can use it in CLI like: `lemon test.lem`
Note: This still uses `<`, it is more of a "hack".
