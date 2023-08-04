// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>

int main(int argc, char **argv)
{
    /*int* sp = (int *) 0x04000000;
    for (int i = 0; i < 100; i++)
        *(sp + i) = i;

    for (int i = 0; i < 100; i++)
        printf("%x\n", *(sp + i));
*/
    printf("Hello from ESP\n");
/*    printf("Testing Mem Tile 0\n");
    int* base = (int *) 0x88000000;
    for (int i = 0; i < 0x08000000 / sizeof(int); i++){
        if (i % 1000000 == 0)
            printf("%d\n", i);
        *(base + i) = i;
    }

    int res;
    int errors = 0;
    for (int i = 0; i < 0x08000000 / sizeof(int); i++){
        if (i % 1000000 == 0)
            printf("%d\n", i);
        res = *(base + i);
        if (res != i)
            errors++;
    }
    printf("errors: %d\n", errors);

    printf("Testing Mem Tile 1\n");
    base = (int *) 0x90000000;
    for (int i = 0; i < 0x8000000 / sizeof(int); i++){
        if (i % 1000000 == 0)
            printf("%d\n", i);
        *(base + i) = i;
    }
    res;
    errors = 0;
    for (int i = 0; i < 0x8000000 / sizeof(int); i++){
        if (i % 1000000 == 0)
            printf("%d\n", i);
        res = *(base + i);
        if (res != i)
            errors++;
    }
    printf("errors: %d\n", errors);
 
    printf("Testing Mem Tile 2\n");
    base = (int *) 0xa0000000;
    for (int i = 0; i < 0x8000000 / sizeof(int); i++){
        if (i % 1000000 == 0)
            printf("%d\n", i);
        *(base + i) = i;
    }
    res;
    errors = 0;
    for (int i = 0; i < 0x8000000 / sizeof(int); i++){
        if (i % 1000000 == 0)
            printf("%d\n", i);
        res = *(base + i);
        if (res != i)
            errors++;
    }
    printf("errors: %d\n", errors);

    printf("Testing Mem Tile 3\n");
    base = (int *) 0xb0000000;
    for (int i = 0; i < 0x8000000 / sizeof(int); i++){
        if (i % 1000000 == 0)
            printf("%d\n", i);
        *(base + i) = i;
    }
    res;
    errors = 0;
    for (int i = 0; i < 0x8000000 / sizeof(int); i++){
        if (i % 1000000 == 0)
            printf("%d\n", i);
        res = *(base + i);
        if (res != i)
            errors++;
    }
    printf("errors: %d\n", errors);
*/
}
