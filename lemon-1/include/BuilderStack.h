#pragma once

// Builder stack for Lemon
#include "llvm/IR/IRBuilder.h"

using namespace llvm;

namespace Lemon {

class BuilderStack {
    std::vector<std::unique_ptr<IRBuilder<>>> builderStack;
    LLVMContext &context;
public:
    BuilderStack(LLVMContext &context): context(context) {}
    ~BuilderStack() {}

    void push(BasicBlock *BB) { builderStack.emplace_back(context); }

    void pop() { builderStack.pop_back(); }
};

}