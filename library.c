#include "library.h"

#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include <memory.h>
#include <unistd.h>

typedef struct{
    uintptr_t* start;
    size_t size;
    enum {
        TYPE_MMAP,
        TYPE_SBRK
    } type;
} allogator_chunk;

static allogator_chunk* free_chunks;
static size_t free_chunks_size;
static size_t free_chunks_capacity;

static allogator_chunk* alloced_chunks;
static size_t alloced_chunks_size;
static size_t alloced_chunks_capacity;

static uintptr_t* last_sbrk_end;
static uintptr_t* stack_start;

allogator_chunk* allogator_chunk_list_expand(allogator_chunk* list, size_t* capacity, size_t new_capacity, size_t size) {
    allogator_chunk* new_list = mmap(NULL, new_capacity * sizeof(allogator_chunk), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    memcpy(new_list, list, size * sizeof(allogator_chunk));
    munmap(list, *capacity * sizeof(allogator_chunk));
    *capacity = new_capacity;
    return new_list;
}

__attribute__((unused)) void* allogator_malloc(size_t size) {
    if (free_chunks_size >= free_chunks_capacity) {
        free_chunks = allogator_chunk_list_expand(free_chunks, &free_chunks_capacity, free_chunks_capacity * 2, free_chunks_size);
    }

    // check if size is bigger than a page and mmap if it is
    if (size > getpagesize()) {
        void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (ptr == MAP_FAILED) {
            return NULL;
        }
        allogator_chunk chunk;
        chunk.start = (uintptr_t*)ptr;
        chunk.size = size;
        chunk.type = TYPE_MMAP;
        free_chunks[free_chunks_size++] = chunk;
        return ptr;
    }

    // find a chunk of the right size
    allogator_chunk* chunk = NULL;
    for (size_t i = 0; i < free_chunks_size; i++) {
        if (free_chunks[i].size >= size) {
            chunk = &free_chunks[i];
            break;
        }
    }

    // if no chunk was found, try to allocate some pages from sbrk
    if (chunk == NULL) {
        // try to allocate 2 pages from sbrk
        void* ptr = sbrk(2 * getpagesize());
        if (ptr == (void*)-1) {
            return NULL;
        }

        // insert the chunk into the list of free chunks
        allogator_chunk new_chunk;
        new_chunk.start = (uintptr_t*)ptr;
        new_chunk.size = 2 * getpagesize();
        new_chunk.type = TYPE_SBRK;

        if (free_chunks_size >= free_chunks_capacity) {
            free_chunks = allogator_chunk_list_expand(free_chunks, &free_chunks_capacity, free_chunks_capacity * 2, free_chunks_size);
        }

        free_chunks[free_chunks_size++] = new_chunk;
        chunk = &free_chunks[free_chunks_size - 1];
        last_sbrk_end = chunk->start + chunk->size;
    }

    // if the chunk must be split, create a new chunk and insert it into the list of free chunks
    if (chunk->size > size) {
        allogator_chunk used_chunk = {
            .start = chunk->start,
            .size = size,
            .type = chunk->type
        };
        chunk->start += size;
        chunk->size -= size;

        if (alloced_chunks_size >= alloced_chunks_capacity) {
            alloced_chunks = allogator_chunk_list_expand(alloced_chunks, &alloced_chunks_capacity, alloced_chunks_capacity * 2, alloced_chunks_size);
        }

        alloced_chunks[alloced_chunks_size++] = used_chunk;
        return used_chunk.start;
    }

    return NULL;
}

__attribute__((unused)) void allogator_free(void* ptr) {
    // find the chunk that contains the pointer
    allogator_chunk* chunk = NULL;
    size_t chunk_index;
    for (size_t i = 0; i < alloced_chunks_size; i++) {
        if (alloced_chunks[i].start == (uintptr_t*)ptr) {
            chunk = &alloced_chunks[i];
            chunk_index = i;
            break;
        }
    }

    // if the chunk is not found, return
    if (chunk == NULL) {
        return;
    }

    // remove the chunk from the list of alloced chunks
    for (size_t i = chunk_index; i < alloced_chunks_size - 1; i++) {
        alloced_chunks[i] = alloced_chunks[i + 1];
    }
    alloced_chunks_size--;

    // insert the chunk into the list of free chunks
    if (free_chunks_size >= free_chunks_capacity) {
        free_chunks = allogator_chunk_list_expand(free_chunks, &free_chunks_capacity, free_chunks_capacity * 2, free_chunks_size);
    }
    free_chunks[free_chunks_size++] = *chunk;

    // if the chunk is a mmap chunk, munmap it
    if (chunk->type == TYPE_MMAP) {
        munmap(chunk->start, chunk->size);
        return;
    }

    // sort the free chunks by address
    for (size_t i = 0; i < free_chunks_size; i++) {
        for (size_t j = i + 1; j < free_chunks_size; j++) {
            if (free_chunks[i].start > free_chunks[j].start) {
                allogator_chunk temp = free_chunks[i];
                free_chunks[i] = free_chunks[j];
                free_chunks[j] = temp;
            }
        }
    }

    return;

    // merge adjacent free chunks
    for (size_t i = 0; i < free_chunks_size - 1; i++) {
        if (free_chunks[i].start + free_chunks[i].size == free_chunks[i + 1].start) {
            free_chunks[i].size += free_chunks[i + 1].size;
            for (size_t j = i + 1; j < free_chunks_size - 1; j++) {
                free_chunks[j] = free_chunks[j + 1];
            }
            free_chunks_size--;
        }
    }

    // reverse through the free chunks and try to give them to the sbrk
    uintptr_t* current_sbrk_end = last_sbrk_end;
    for (size_t i = free_chunks_size; i > 0; i--) {
        if (free_chunks[i - 1].start + free_chunks[i - 1].size == last_sbrk_end) {
            last_sbrk_end = free_chunks[i - 1].start;
            for (size_t j = i; j < free_chunks_size; j++) {
                free_chunks[j - 1] = free_chunks[j];
            }
            free_chunks_size--;
        }
    }

    if (last_sbrk_end < current_sbrk_end) {
        sbrk(-(current_sbrk_end - last_sbrk_end));
    }

    // if the allocated chunks capacity is twice as big as the number of allocated chunks, shrink the list
    if (alloced_chunks_capacity > alloced_chunks_size * 2 && alloced_chunks_size > 1) {
        alloced_chunks = allogator_chunk_list_expand(alloced_chunks, &alloced_chunks_capacity, alloced_chunks_capacity / 2, alloced_chunks_size);
    }

    // if the free chunks capacity is twice as big as the number of free chunks, shrink the list
    if (free_chunks_capacity > free_chunks_size * 2 && free_chunks_size > 1) {
        free_chunks = allogator_chunk_list_expand(free_chunks, &free_chunks_capacity, free_chunks_capacity / 2, free_chunks_size);
    }
}

void allogator_dump_chunks() {
    printf("Allocated chunks:\n");
    for (size_t i = 0; i < alloced_chunks_size; i++) {
        printf("%p: %zu\n", alloced_chunks[i].start, alloced_chunks[i].size);
    }
    printf("Free chunks:\n");
    for (size_t i = 0; i < free_chunks_size; i++) {
        printf("%p: %zu\n", free_chunks[i].start, free_chunks[i].size);
    }

    printf("Last sbrk end: %p\n", last_sbrk_end);
}

__attribute__((unused)) void* allogator_realloc(void* ptr, size_t size) {
    printf("realloc(%p, %zu)\n", ptr, size);
    return NULL;
}

__attribute__((unused)) void init() {
    free_chunks = mmap(NULL, sizeof(allogator_chunk), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    free_chunks_size = 0;
    free_chunks_capacity = 1;

    alloced_chunks = mmap(NULL, sizeof(allogator_chunk), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    alloced_chunks_size = 0;
    alloced_chunks_capacity = 1;

    // align sbrk to page size
    void* ptr = sbrk(0);
    uintptr_t sbrk_start = (uintptr_t)ptr;
    uintptr_t sbrk_end = sbrk_start + getpagesize();
    long long delta = (long long) sbrk_end - (long long) sbrk_start;
    sbrk(delta);

    void* curr_brk = sbrk(0);
    last_sbrk_end = (uintptr_t*)curr_brk;

    // set the stack start for the gc thread
    __asm__("movq %%rsp, %0" : "=r"(stack_start));
}
