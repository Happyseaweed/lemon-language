#include "../include/Tensor.h"


Tensor* tensor_create(const int* shape, int ndims) {
    Tensor* t   = (Tensor*)malloc(sizeof(Tensor));
    t->ndims    = ndims;
    t->shape    = (int*)malloc(ndims * sizeof(int));
    t->strides  = (int*)malloc(ndims * sizeof(int));

    // Calculate total elements:
    int num_elements = 1;
    for (int i = 0; i < ndims; ++i) {
        num_elements *= shape[i];
        
        t->shape[i] = shape[i];
        // Strides calculation
        t->strides[i] = (i == ndims - 1) ? 1 : t->strides[i+1] * t->shape[i+1];
    }

    // initialize all to 0
    t->data = (double*)malloc(num_elements * sizeof(double));
    memset(t->data, 0, num_elements * sizeof(double));
    return t;
}

void tensor_free(Tensor* t){ 
    free(t->data);
    free(t->strides);
    free(t->shape);
    free(t);
}

double* tensor_at(Tensor* t, const int* indices) {
    int offset = 0;
    for (int i = 0; i < t->ndims; ++i) {
        offset += indices[i] * t->strides[i];
    }
    
    return &t->data[offset];
}