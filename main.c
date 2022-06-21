#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "library.h"

int main() {
    // create an array of 1000 ints with the custom allocator
    int* array = allogator_malloc(100 * sizeof(int));
    allogator_dump_chunks();

    printf("----------------------------------------------------\n");

    // fill the array with random values
    for (size_t i = 0; i < 100; i++) {
        array[i] = rand();
    }

    // free the array
    allogator_free(array);

    // dump the chunks
    allogator_dump_chunks();

    printf("----------------------------------------------------\n");

    // allocate some memory
    void* ptr = allogator_malloc(100 * sizeof(int));

    // allocate some more memory
    void* ptr2 = allogator_malloc(100 * sizeof(int));

    allogator_free(ptr);

    allogator_dump_chunks();

    printf("----------------------------------------------------\n");

    // free the second pointer
    allogator_free(ptr2);

    allogator_dump_chunks();
}