#include "../include/system.h"
#include "../include/stdlib.h"
#include <stddef.h>

#define ALLOC_MIN_PAGES 8

extern char _end;
static const void* heap_start = (void*)((uint64_t)(&_end)&~0xfff + 0x1000);

uint64_t heap_size = 0;

typedef struct malloc_list{
    struct malloc_list* next;
    size_t size;
} malloc_list;

struct malloc_list head = {0,0};

void *palloc(size_t pages)
{
    uint64_t pos = (uint64_t)(&heap_start) + (heap_size << 12ULL);
    heap_size += pages;
    get_page_multi(pos, pages);
    return (void*)pos;
}

void *malloc_int(size_t size_bytes, size_t* size_bytes_a)
{
    // Reserve 8 bytes for size header
    size_bytes += 8;
    // Allign to 16
    *size_bytes_a = size_bytes & ~0x0fULL;
    if (size_bytes%16) *size_bytes_a += 16-(*size_bytes_a)%16;
    struct malloc_list* l = &head;
    while (l->next != 0 && l->next->size < (*size_bytes_a)) {
        l = l->next;
    }
    if (l->next == 0) {
        size_t pages_needed = (*size_bytes_a)/4096;
        if ((*size_bytes_a)%4096) pages_needed += 1;
        if (pages_needed < ALLOC_MIN_PAGES) pages_needed = ALLOC_MIN_PAGES;
        l->next = (struct malloc_list*)palloc(pages_needed);
        l->next->next = 0;
        l->next->size = pages_needed*4096;
    } 

    if (l->next->size == *size_bytes_a) {
        uint64_t* p = (uint64_t*)l->next;
        l->next = l->next->next;
        p[0] = *size_bytes_a;
        return p;
    } else {
        uint64_t* p = (uint64_t*)l->next;
        struct malloc_list* new_e = (struct malloc_list*)((char*)p + (*size_bytes_a));
        new_e->size = l->next->size - (*size_bytes_a);
        new_e->next = l->next->next;
        l->next = new_e;
        p[0] = *size_bytes_a;
        return p;
    }
}

/*
void *realloc(void *, size_t);
void *calloc(size_t nelem, size_t size)
{
    size_t total_size = nelem * size;
    size_t inited;
    uint64_t* ptr = (uint64_t*)malloc_int(total_size, inited);
    if (ptr != 0) memset(ptr+1, inited/8 - 1);
    return &ptr[1];
}
*/

void *malloc(size_t size)
{
    size_t l;
    return (char*)malloc_int(size, &l) + 8;
}

void free(void * p)
{
    if (p == 0) return;

    uint64_t* base = (uint64_t*)p;
    --base;
    uint64_t size = *base;

    struct malloc_list* k = (struct malloc_list*)base;
    k->next = head.next;
    head.next = k;
    k->size = size;
}