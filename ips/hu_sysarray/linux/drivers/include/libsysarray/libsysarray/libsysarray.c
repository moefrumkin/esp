#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include "libsysarray.h"

long sysarray_matmul(int fd) {
    return ioctl(fd, SYSARRAY_RUN_MATMUL);
}

long sysarray_load_weights(int fd, quantized* start) {
    return ioctl(fd, SYSARRAY_LOAD_WEIGHTS, start);
}

long sysarray_load_input(int fd, quantized* start) {
    return ioctl(fd, SYSARRAY_LOAD_INPUT, start);
}

long sysarray_store_output(int fd, quantized* start) {
    return ioctl(fd, SYSARRAY_STORE_OUTPUT, start);
}

long sysarray_config(int fd, int config) {
    return ioctl(fd, SYSARRAY_SET_CONFIG, config);
}

long sysarray_flip_mem(int fd, int flip) {
    return ioctl(fd, SYSARRAY_FLIP_MEM, flip);
}

struct sysarray_allocation* sysarray_alloc_buffer(int fd, int size) {
    printf("Calling Sysarray Allocation, for size %d\n", size);

    long rc;
    struct sysarray_allocation *buffer = malloc(sizeof(struct sysarray_allocation));

    buffer->size = size;
    
    rc = ioctl(fd, SYSARRAY_ALLOC_BUFFER, buffer); 

    if(rc) {
        printf("Syscall failed :(\n");
        return NULL;
    }

    printf("Received Block, physical %p, kernel %p\n", buffer->physical, buffer->kernel);

    int mem_file = open("/dev/mem", O_RDWR);

    if(mem_file == 0) {
        perror("Unable to Open Memory Device \n");
        ioctl(fd, SYSARRAY_FREE_BUFFER, buffer);
        return NULL;
    }

    printf("Memory file opened\n");

    void *base_pointer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_file, (unsigned long) buffer->physical);
    close(mem_file);

    if(base_pointer == NULL) {
        ioctl(fd, SYSARRAY_FREE_BUFFER, buffer);
        return NULL;
    }

    buffer->user = base_pointer;
    return buffer;
}

//needs better error handling
void sysarray_free_buffer(int fd, struct sysarray_allocation *buffer) {
    ioctl(fd, SYSARRAY_FREE_BUFFER, buffer);

    int mem_file = open("/dev/mem/", O_RDWR);
    munmap(buffer->user, buffer->size);
    close(mem_file);
}

long sysarray_gemm(int fd, quantized* A, quantized *B, int N0, int M, int N1, quantized* out) {
    struct sysarray_gemm_config gemm = {
        .A = A,
        .B = B,
        .N0 = N0,
        .M = M,
        .N1 = N1,
        .out = out,
    };

    return ioctl(fd, SYSARRAY_GEMM, &gemm);
}
