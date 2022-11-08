#include "malloc.hh"
#include "utils.hh"
#include "palloc.hh"
#include "asm.hh"

malloc_list head = {nullptr, 0};

void print_list(malloc_list* l)
{
    while (l != nullptr) {
        t_print_bochs(" node %h size %h", l, l->size);
        l = l->next;
    }

    t_print_bochs("\n");
}

void *malloc_int(size_t size_bytes, size_t& size_bytes_a)
{
    if (size_bytes == 0) halt();
    // Reserve 8 bytes for size header
    size_bytes += 8;
    // Allign to 16
    size_bytes_a = size_bytes & ~0x0f;
    if (size_bytes%16) size_bytes_a += 16-size_bytes_a%16;
    malloc_list* l = &head;
    while ((l->next != nullptr) and (l->next->size < size_bytes_a)) {
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
        u64* p = (u64*)l->next;
        l->next = l->next->next;
        p[0] = size_bytes_a;
        return p;
    } else {
        u64* p = (u64*)l->next;
        malloc_list* new_e = (malloc_list*)((char*)p + size_bytes_a);
        new_e->size = l->next->size - size_bytes_a;
        new_e->next = l->next->next;
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
    u64* ptr = (u64*)malloc_int(total_size, inited);
    if (ptr != nullptr) memset(ptr+1, inited/8 - 1);
    return &ptr[1];
}

void *malloc(size_t s)
{
    size_t l;
    void* p = ((char*)malloc_int(s, l) + 8);
    return p;
}

void free(void * p)
{
    if (p == nullptr) return;

    if ((u64)p < 0x1000) halt();

    u64* base = (u64*)p;
    --base;
    u64 size = *base;

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

 void operator delete(void *p, UNUSED size_t s)
{
    free(p);
}
 
void operator delete[](void *p, UNUSED size_t s)
{
    free(p);
}

void *operator new(size_t, void *p)     noexcept { return p; }
void *operator new[](size_t, void *p)   noexcept { return p; }
void  operator delete  (void *, void *) noexcept { };
void  operator delete[](void *, void *) noexcept { };