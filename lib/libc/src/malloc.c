#include <pmos/system.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <pmos/memory.h>

#define ALLOC_MIN_PAGES 8

size_t heap_size = 0;

typedef struct malloc_list{
    struct malloc_list* next;
    size_t size;
} malloc_list;

struct malloc_list head = {0,0};

static size_t get_alloc_size(const void* ptr)
{
    return *(((const uint64_t*)ptr) - 1);
}

void *palloc(size_t pages)
{
    mem_request_ret_t req = create_normal_region(0, NULL, pages*4096, PROT_READ | PROT_WRITE);
    if (req.result != SUCCESS)
        return NULL;

    return req.virt_addr;
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

void *realloc(void *old_ptr, size_t new_size)
{
    void* new_ptr = NULL;
    if (new_size != 0) {
        new_ptr = malloc(new_size);
    }

    if (old_ptr != NULL) {
        if (new_ptr != NULL)
            memcpy(new_ptr, old_ptr, get_alloc_size(old_ptr));

        free(old_ptr);
    }
    
    return new_ptr;
}

void *calloc(size_t num, size_t size)
{
    size_t total_size = num*size;
    void* ptr = malloc(total_size);
    
    if (ptr != NULL)
        memset(ptr, 0, total_size);
    
    return ptr;
}

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

