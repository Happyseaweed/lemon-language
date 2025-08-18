#pragma once

#include <string.h>
#include <stdlib.h>

typedef struct {
    int ndims;
    int *shape;
    int *strides;   // Might not be 100% necessary
    double* data;
} Tensor;

Tensor* tensor_create(const int* shape, int ndims);

void tensor_free(Tensor* t);

double* tensor_at(Tensor* t, const int* indices);