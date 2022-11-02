#pragma once
#include <stddef.h>

void *malloc(size_t);
void *realloc(void *, size_t);
void *calloc(size_t, size_t);
void free(void *);

void *operator new(size_t size);
void *operator new[](size_t size);
void operator delete(void *p);
void operator delete[](void *p);

void *operator new(size_t, void *p) noexcept;
void *operator new[](size_t, void *p) noexcept;
void  operator delete  (void *, void *) noexcept;
void  operator delete[](void *, void *) noexcept;

#define ALLOC_MIN_PAGES 4

struct malloc_list {
    malloc_list* next = nullptr;
    size_t size = 0;
};