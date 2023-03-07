#ifndef _STDLIB_H
#define _STDLIB_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long size_t;

void abort()  __attribute__((noreturn));
void free(void* ptr);
void* malloc(size_t size);
void *calloc(size_t nitems, size_t size);

#define alloca(x) __builtin_alloca (x)

char *getenv(const char *name);

inline int sched_yield(void)
{
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif