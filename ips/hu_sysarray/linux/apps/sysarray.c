// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
/* User-defined code */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include "libsysarray.h"

#define NAME "/dev/sysarray_rtl.0"

#define M 32

#define MAX_PRINTED_ERRORS 0

const static int IsRelu = 1;
const static int BiasShift = 6;
const static int AccumShift = 10;
const static int AccumMul = 93;

static void init_buf(char *B_mat, char *W_mat, char *I_mat, char *O_mat) {
#include "input_B.h"
#include "input_W.h"
#include "input_I.h"
#include "gold.h"
}

static int validate_buf(char *out, char *gold, int out_len)
{
    int j;
    char val;
    unsigned errors = 0;

    for (j = 0; j < out_len; j++) {
        val = out[j];
        if (gold[j] != val) {
            errors++;
            if(errors == 1)
                printf("# | Act | Exp\n");
            if (errors <= MAX_PRINTED_ERRORS) {
                printf("%d : %d : %d\n", j, val, gold[j]);
            }
        }
    }

    return errors;
}

static void print_start(int num, quantized *ptr) {
    int i;
    for(i = 0; i < num; i++) {
        printf("arg[%d]: %d\n", i, *(ptr++));
    }
}

int main() {
#define SOC_TILES 36
#define CSR_TILE_SIZE 0x200 
#define CSR_BASE_ADDR 0x60090000
#define SYSARRAY_TILE_NUM 19

    struct sysarray_allocation *alloc;
    int fd;

    fd = open("/dev/mem", O_RDWR); 
    void* csr_base_ptr = mmap(NULL, SOC_TILES * CSR_TILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, CSR_BASE_ADDR);
    close(fd); 

    int* coherence_reg_ptr = ((int*) csr_base_ptr) + ((CSR_TILE_SIZE * SYSARRAY_TILE_NUM + 0x190) / sizeof(int));
    *coherence_reg_ptr = 2; 

    printf("Testing the Systolic Array!\n");

    printf("Sizeof quantization: %d\n", sizeof(quantized));

    printf("Loading Driver\n");

    fd = open(NAME, O_RDWR, 0);

    if(fd < 0) {
        printf("unable to open driver file\n");
        return fd;
    }

    printf("Done!\n");

    int bias_len = 32;
    int weight_len = 32 * 32;
    int input_len = 32 * 32;
    int output_len = 32 * 32;

    int bias_offset = 0;
    int weight_offset = bias_offset + bias_len;
    int input_offset = weight_offset + weight_len;
    int output_offset = input_offset + input_len;

    int mem_size = output_offset + output_len;
    /*const unsigned long address = 0xB0000000;

    //char *mem = aligned_malloc(mem_size);

    void *monitor_base_ptr;
    int mem_file = open("/dev/mem", O_RDWR);
    monitor_base_ptr = mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_file, address);
    quantized* mem = (quantized *) monitor_base_ptr;
    close(mem_file);*/

    printf("Allocating Buffer\n");

    quantized* mem;
    long address;

    alloc = sysarray_alloc_buffer(fd, mem_size);

    if(alloc == NULL) {
        printf("Unable to Allocate Buffer\n");
        return -1;
    }

    printf("Buffer Allocated At, %p physical, %p kernel, %p user\n", alloc->physical, alloc->kernel, alloc->user);

    mem = (quantized*) alloc->user;
    address = alloc->physical;

    printf("Mem starts at %p\n", mem);

    char* gold = malloc(output_len);

    printf("Loading Test Data\n");

    init_buf(&mem[bias_offset], &mem[weight_offset], &mem[input_offset], gold);
    
    printf("Done!\n");


    unsigned data = 0;

    data += M - 1;
    data += IsRelu << 8;
    data += BiasShift << 16;
    data += AccumShift << 20;
    data += AccumMul << 24;

    printf("Configuring Sysarray, with %x\n", data);

    //ioctl(fd, SYSARRAY_SET_CONFIG, data);
    sysarray_config(fd, data);

    printf("Done!\n");

    printf("Weight base %p, Input base %p, Output base %p\n", address, address + input_offset, address + output_offset);

    struct timespec mt1, mt2;
    long int tt;

    clock_gettime (CLOCK_REALTIME, &mt1);

    //ioctl(fd, SYSARRAY_LOAD_WEIGHTS, address);
    sysarray_load_weights(fd, address);


    //ioctl(fd, SYSARRAY_LOAD_INPUT, address + input_offset);
    sysarray_load_input(fd, address + input_offset);

    //ioctl(fd, SYSARRAY_RUN_MATMUL, 0);
    sysarray_matmul(fd);

    //ioctl(fd, SYSARRAY_STORE_OUTPUT, address + output_offset);
    sysarray_store_output(fd, address + output_offset);

    clock_gettime (CLOCK_REALTIME, &mt2);

    tt=1000000000*(mt2.tv_sec - mt1.tv_sec)+(mt2.tv_nsec - mt1.tv_nsec);

    printf("Sysarray completed in %d nanoseconds\n", tt);

    int errors = validate_buf(&mem[output_offset], gold, output_len);

    if (errors)
        printf("Sysarray Test Failed\n");
    else
        printf("Sysarray Test Passed!\n");

    /*

    ///TEST ON CPU
    int i, j, k;
    quantized acc;

    clock_gettime (CLOCK_REALTIME, &mt1);

    for(i = 0; i < 32; i++) {
        for(j = 0; j < 32; j++) {
            acc = 0;
            for(k = 0; k < 32; k++) {
                acc += mem[weight_offset + (i * 32 + k)] * mem[input_offset + (k * 32 + j)];
            }
            acc += mem[bias_offset + i];
            mem[output_offset + (i * 32 + j)] = acc;
        }
    }

    clock_gettime (CLOCK_REALTIME, &mt2);

    tt=1000000000*(mt2.tv_sec - mt1.tv_sec)+(mt2.tv_nsec - mt1.tv_nsec);

    printf("CPU completed in %d nanoseconds\n", tt);

    errors = validate_buf(&mem[output_offset], gold, output_len);

    sysarray_free_buffer(fd, alloc);

    if (errors)
        printf("Test Failed\n");
    else
        printf("Test Passed!\n");*/

    return 0;
}
