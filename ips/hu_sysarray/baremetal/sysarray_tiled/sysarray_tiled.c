/* Copyright (c) 2011-2021 Columbia University, System Level Design Group */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#ifndef __riscv
#include <stdlib.h>
#endif

#include <esp_accelerator.h>
#include <esp_probe.h>
#include "sysarray_minimal.h"

typedef char token_t;
typedef char native_t;

static unsigned DMA_WORD_PER_BEAT(unsigned _st)
{
    return (sizeof(void *) / _st);
}

#define MAX_PRINTED_ERRORS 10

#define HU_SYSARRAY  0x102
#define DEV_NAME "hu,hu_sysarray"

static unsigned in_len1;
static unsigned in_len2;
static unsigned in_len3;
static unsigned in_offset1;
static unsigned in_offset2;
static unsigned in_offset3;
static unsigned out_len;
static unsigned in_size1;
static unsigned in_size2;
static unsigned out_size;
static unsigned out_offset;
static unsigned mem_size;

// configuration parameters
const static int IsRelu = 1;
const static int BiasShift = 6;
const static int AccumShift = 10;
const static int AccumMul = 93;
// start signal (write only), fire interrupt upon completion
const static int MWR   = 1; // master weight read
const static int MDR   = 2; // master input read
const static int MDW   = 3; // master input write
const static int START = 4; // start systolic array

// configuration register offsets
#define Dummy_REG    0x00
#define SA_START     0x04
#define SA_CONFIC    0x08
// base address in DRAM for weight mem, data read mem and data write mem
#define SA_W_RD_BASE 0x0C
#define SA_D_RD_BASE 0x10
#define SA_D_WR_BASE 0x14

#define HEIGHT 32
#define WIDTH 32

static long sysarray_gemm(esp_device *dev, token_t *W, token_t *A, token_t *out, int N0, int M, int N1); 

// Used to calculate time (get current counter)
static inline uint64_t get_counter() {
    uint64_t counter;
    asm volatile (
            "li t0, 0;"
            "csrr t0, mcycle;"
            "mv %0, t0"
            : "=r" ( counter )
            :
            : "t0"
            );
    return counter;
}


static void iointerrupt()
{
    printf("wait\n");
}

// output validation
static int validate_buf(token_t *out, native_t *gold)
{
    int j;
    native_t val;
    unsigned errors = 0;

    for (j = 0; j < out_len; j++) {
        val = out[j];
        if (gold[j] != val) {
            errors++;
            if (errors <= MAX_PRINTED_ERRORS) {
                printf("%d : %d : %d\n", j, val, gold[j]);
            }
        }
    }

    return errors;
}

// input and expected output initialization
static void init_buf(token_t *B_mat, token_t *W_mat, token_t *I_mat, native_t *O_mat)
{
#include "input_B.h"
#include "input_W.h"
#include "input_I.h"
#include "gold.h"
}

int main(int argc, char * argv[])
{
    int i;
    int n;
    int ndev;
    struct esp_device dev, coh_dev;
    unsigned done;
    token_t *mem;
    native_t *gold;
    unsigned errors = 0;
    unsigned coherence;
    unsigned data = 0, data_read = 0;

    // base address
    unsigned mask_rd_base;
    unsigned input_rd_base;
    unsigned aux_rd_base;
    unsigned mask_wr_base;
    unsigned input_wr_base;

    int num_interrupts = 0;

    // base address
    unsigned w_rd_base;
    unsigned d_rd_base;
    unsigned d_wr_base;

    // non const parameters
    data += (32-1);  // M_1
    data += IsRelu << 8;
    data += BiasShift << 16;
    data += AccumShift << 20;
    data += AccumMul << 24;

    // offsets in data buffer

    in_len1 = 1024;    // bias
    in_len2 = 1024*1024; // weight      
    in_len3 = 1024*1024; // activation
    out_len = 1024*1024;

    in_offset1 = 0;
    in_offset2 = in_offset1 + in_len1;
    in_offset3 = in_offset2 + in_len2;
    out_offset  = in_offset3 + in_len3;

    in_size1 = (in_len1 + in_len2) * sizeof(token_t);
    in_size2 = (in_len3) * sizeof(token_t);
    out_size = out_len * sizeof(token_t);

    mem_size = (out_offset * sizeof(token_t)) + out_size;

    dev.addr = ACC_ADDR;

    // TO MODIFY: Allocate memory
    // Allocation of the accelerator data array (mem) and of the expected output array (gold)
    mem = aligned_malloc(mem_size);
    gold = aligned_malloc(out_size);

    w_rd_base = ((unsigned) mem);
    d_rd_base = ((unsigned) mem) + in_size1;
    d_wr_base = ((unsigned) mem) + in_size1 + in_size2;

    printf("  Generate input...\n");

    init_buf(&mem[in_offset1], &mem[in_offset2], &mem[in_offset3], gold);

    // Flush (customize coherence model here)
    coherence = ACC_COH_RECALL;
    coh_dev.addr = CSR_TILE_ADDR;
    iowrite32(&coh_dev, CSR_REG_OFFSET*4, coherence);
    if (coherence != ACC_COH_RECALL)
        esp_flush(coherence);

    // Write the accelerator configuration registers
    iowrite32(&dev, SA_CONFIC, data);

    uint64_t start, end;

    start = get_counter();

    sysarray_gemm(&dev, w_rd_base, d_rd_base, d_wr_base, 1024, 1024, 1024);

    end = get_counter();

    printf("Compelted in %d clock cycles.\n", end - start);

    /* Validation */
    errors = validate_buf(&mem[out_offset], gold);

    if (errors)
        printf("  ... FAIL\n");
    else
        printf("  ... PASS\n");

    aligned_free(mem);
    aligned_free(gold);

    return 0;
}

static void load_block(token_t *target, token_t *source, int M, int N, int m, int n, int col, int row) {
    int i, j;
    //the ordering of these loops should matter for cache locality
    //perhaps we could optimize by using larger reads than chars, but we would then need the dimensions to be multiples of the larger types' width in bytes.
    for(i = 0; i < m; i++) {
        for(j = 0; i < n; j++) {
            target[i * n + j] = source[(row + i) * N + (col + j)];
        }
    }
}

static void store_block(token_t *target, token_t *source, int M, int N, int m, int n, int row, int col) {
    int i, j;
    for(i = 0; i < m; i++) {
        for(j = 0; j < n; j++) {
            source[(row + i) * N + (col + j)] = target[i * n + j];
        }
    }
}


static void accumulate(token_t *acc, token_t *src, int len) {
    int i;
    for(i = 0; i < len; i++)
        acc[i] += src[i];
}

static long sysarray_gemm(esp_device *dev, token_t *W, token_t *A, token_t *out, int N0, int M, int N1) {
    int astrides, shared_strides, wstrides;
    int n0, m, n1; //block sizes
    int rc;

    token_t *weights, activations;
    token_t *out; 

    token_t *wblock, ablock; //we need the blocks since blocks in a larger matrix are not contiguous in memory
    token_t *acc, *scratch; //width * height. In theory, we could get rid of acc with a function that accumulates a block straight into the output

    printf("Running GEMM\n");

    printk("Dimensions are (%d x %d), (%d, %d)\n", N0, M, M, N1);

    shared_strides = wstrides = (M + (HEIGHT - 1)) / HEIGHT; //TODO: Check height constraints
    astrides = (M + (WIDTH - 1)) / WIDTH;

    n0 = m = WIDTH;
    n1 = WIDTH;

    int wblock_size = n0 * m;
    int ablock_size = m * n1;

    printf("wstrides %d, shared_strides %d, astrides %d\n", wstrides, shared_strides, astrides);

    printf("Block Dimensions are (%d x %d) X (%d x %d)\n", n0, m, m, n1); 

    wblock = aligned_malloc(wblock_size);
    ablock = aligned_malloc(ablock_size);
    scratch = aligned_malloc(n0 * n1);
    out = aligned_malloc(n0 * n1);

    int q, r, i;

    // Write the accelerator configuration registers
    //iowrite32(&dev, SA_CONFIC, data);
    iowrite32(&dev, SA_W_RD_BASE, wblock);
    iowrite32(&dev, SA_D_RD_BASE, ablock);
    iowrite32(&dev, SA_D_WR_BASE, scratch);

    for(q = 0; q < wstrides; q++) {
        for(r = 0; r < astrides; r++) {
            //printk(KERN_DEBUG "Computing output block (%d, %d)\n", q, r);
            // zero the accumulator
            memset(acc, 0, astrides * wstrides);
            for(i = 0; i < shared_strides; i++) {
                load_block(wblock, weights, N0, M, n0, m, q * n0, i * m);
                load_block(ablock, weights, M, N1, m, n1, i * m, r * n1);

                iowrite32(&dev, MDR);
                iowrite32(&dev, MWR);
                iowrite32(&dev, START);
                iowrite32(&dev, MDW);

                accumulate(acc, scratch, n0 * n1);
            }

            store_block(acc, out, N0, N1, n0, n1, q * n0, r * n1);
        }
    }

    aligned_free(wblock);
    aligned_free(ablock);
    aligned_free(acc);
    aligned_free(scratch);
}
