#include "malloc.hh"
#include "utils.hh"
#include "palloc.hh"
#include "asm.hh"

malloc_list head = {0,0};

void *malloc_int(size_t size_bytes, size_t& size_bytes_a)
{
    // Reserve 8 bytes for size header
    size_bytes += 8;
    // Allign to 16
    size_bytes_a = size_bytes & ~0x0f;
    if (size_bytes%16) size_bytes_a += 16-size_bytes_a%16;
    t_print("Debug: malloc %i bytes (alligned %i)\n", size_bytes, size_bytes_a);
    malloc_list* l = &head;
    while (l->next != nullptr and l->next->size < size_bytes_a) {
        l = l->next;
    }
    if (l->next == nullptr) {
        size_t pages_needed = size_bytes_a/4096;
        if (size_bytes_a%4096) pages_needed += 1;
        if (pages_needed < ALLOC_MIN_PAGES) pages_needed = ALLOC_MIN_PAGES;

        l->next = (malloc_list*)palloc(pages_needed);
        l->next->next = nullptr;
        l->next->size = pages_needed*4096;
    } 

    if (l->next->size == size_bytes_a) {
        uint64_t* p = (uint64_t*)l->next;
        l->next = l->next->next;
        p[0] = size_bytes_a;
        return p;
    } else {
        uint64_t* p = (uint64_t*)l->next;
        malloc_list* new_e = (malloc_list*)((char*)p + size_bytes_a);
        new_e->size = l->next->size - size_bytes_a;
        new_e->next = l->next;
        l->next = new_e;
        p[0] = size_bytes_a;
        return p;
    }
}

void *realloc(void *, size_t);
void *calloc(size_t nelem, size_t size)
{
    size_t total_size = nelem * size;
    size_t inited;
    uint64_t* ptr = (uint64_t*)malloc_int(total_size, inited);
    if (ptr != nullptr) memset(ptr+1, inited/8 - 1);
    return &ptr[1];
}

void *malloc(size_t s)
{
    size_t l;
    return (char*)malloc_int(s, l) + 8;
}

void free(void * p)
{
    uint64_t* base = (uint64_t*)p;
    --base;
    uint64_t size = *base;

    malloc_list* k = (malloc_list*)base;
    k->next = head.next;
    head.next = k;
    k->size = size;
}
 
void *operator new(size_t size)
{
    return malloc(size);
}
 
void *operator new[](size_t size)
{
    return malloc(size);
}
 
 void operator delete(void *p)
{
    free(p);
}
 
void operator delete[](void *p)
{
    free(p);
}