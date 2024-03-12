#pragma once
#include <stddef.h>

extern "C" void *malloc(size_t);
extern "C" void *realloc(void *, size_t);
extern "C" void *calloc(size_t, size_t);
extern "C" void free(void *);

void *operator new(size_t size);
void *operator new[](size_t size);
void operator delete(void *p);
void operator delete[](void *p);
void operator delete(void *p, size_t size);
void operator delete[](void *p, size_t size);

void *operator new(size_t, void *p) noexcept;
void *operator new[](size_t, void *p) noexcept;
void  operator delete  (void *, void *) noexcept;
void  operator delete[](void *, void *) noexcept;