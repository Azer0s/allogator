#ifndef ALLOGATOR_LIBRARY_H
#define ALLOGATOR_LIBRARY_H

#include <stddef.h>

__attribute__((unused)) void* allogator_malloc(size_t size);
__attribute__((unused)) void allogator_free(void* ptr);
__attribute__((unused)) void* allogator_realloc(void* ptr, size_t size);

__attribute__((unused)) __attribute__((constructor)) void init();

void allogator_dump_chunks();

#endif //ALLOGATOR_LIBRARY_H
