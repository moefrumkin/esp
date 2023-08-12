#include <linux/ioctl.h>

#define SYSARRAY_MAGIC 'S'

typedef unsigned char quantized;

#define SYSARRAY_RUN_MATMUL _IO(SYSARRAY_MAGIC, 0)             
#define SYSARRAY_LOAD_WEIGHTS _IOWR(SYSARRAY_MAGIC, 1, unsigned)
#define SYSARRAY_LOAD_INPUT _IOWR(SYSARRAY_MAGIC, 2, unsigned) 
#define SYSARRAY_STORE_OUTPUT _IOWR(SYSARRAY_MAGIC, 3, unsigned)
#define SYSARRAY_SET_CONFIG _IOW(SYSARRAY_MAGIC, 4, unsigned)  
#define SYSARRAY_FLIP_MEM _IOW(SYSARRAY_MAGIC, 5, unsigned)   

//(N0 x M) x (M x N1)
struct sysarray_gemm_config {
    quantized* A;
    quantized* B;
    int N0;
    int M;
    int N1;
    quantized* out;
};

#define SYSARRAY_GEMM _IOWR(SYSARRAY_MAGIC, 6, struct sysarray_gemm_config)                                                                                                                                 
struct sysarray_allocation {
    int size;
    void* user;
    void* kernel;
    void* physical;
};

#define SYSARRAY_ALLOC_BUFFER _IOWR(SYSARRAY_MAGIC, 7, struct sysarray_allocation)
#define SYSARRAY_FREE_BUFFER _IOWR(SYSARRAY_MAGIC, 8, struct sysarray_allocation)

long sysarray_matmul(int fd);
long sysarray_load_weights(int fd, quantized* start);
long sysarray_load_input(int fd, quantized* start);
long sysarray_store_output(int fd, quantized* start);
long sysarray_config(int fd, int config);
long sysarray_flip_mem(int fd, int flip);

struct sysarray_allocation* sysarray_alloc_buffer(int fd, int size);
void sysarray_free_buffer(int fd, struct sysarray_allocation *buffer);

long sysarray_gemm(int fd, quantized* A, quantized *B, int N0, int M, int N1, quantized* out);
