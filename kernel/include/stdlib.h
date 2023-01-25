#ifndef _STDLIB_H
#define _STDLIB_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long size_t;

void abort();
void free(void* ptr);
void* malloc(size_t size);
void *calloc(size_t nitems, size_t size);

inline int sched_yield(void)
{
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif