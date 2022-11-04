#ifndef _STDLIB_H
#define _STDLIB_H 1
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

void *malloc(size_t size);
void free(void*);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif