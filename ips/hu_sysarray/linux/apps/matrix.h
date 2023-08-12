#include "libsysarray.h"

enum mat_order {
    ROW_MAJOR,
    COL_MAJOR
};

typedef struct matrix {
    int m;
    int n;
    quantized *values;
} Matrix;

Matrix* mat_alloc(int m, int n, enum mat_order order);
Matrix* mat_alloc_zero(int m, int n, enum mat_order);

Matrix* transpose(Matrix *mat);

Matrix* multiply(Matrix *A, Matrix *B);
