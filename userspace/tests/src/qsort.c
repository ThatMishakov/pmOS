#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#define ARRAY_SIZE 1000

int compare(const void * a, const void * b) {
    return ( *(int*)a - *(int*)b );
}

void test_qsort() {
    printf("Testing qsort...\n");

    static int srand_seed = 1234;

    for (int i = 0; i < 100; ++i) {
        srand(srand_seed);
        srand_seed = rand();

        int values[ARRAY_SIZE];

        for(int i = 0; i < ARRAY_SIZE; i++) {
            values[i] = rand(); // Generate random numbers between 0 and 9999
        }

        qsort(values, ARRAY_SIZE, sizeof(int), compare);

        // Check that the array is sorted in ascending order
        for(int i = 0; i < ARRAY_SIZE - 1; i++) {
            assert(values[i] <= values[i + 1]);
        }

        // for (int i = 0; i < ARRAY_SIZE; i++) {
        //     printf("%d ", values[i]);
        // }
    }
}