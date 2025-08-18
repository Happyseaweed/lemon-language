// Simple typing system

#pragma once
#include <vector>

namespace LemonType {

enum class TypeKind {
    Double,
    Tensor,
    String
};

struct Type {
    TypeKind kind;
    std::vector<size_t> shape;  // Empty for scalar and strings

    const bool isScalar() const { return kind == TypeKind::Double; }
    const bool isTensor() const { return kind == TypeKind::Tensor; }
    const bool isString() const { return kind == TypeKind::Tensor; }
    const std::vector<size_t> getTypeShape() const { return shape; }
};

}