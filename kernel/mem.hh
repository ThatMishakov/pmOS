#pragma once
#include <stdint.h>

class PFrameAllocator {
public:
    void free(void* page);

    // Returns -1 if not found
    void* alloc_page();
    uint64_t alloc_page_ppn();
    void reserve(void* base, uint64_t size);

    void init(uint64_t * bitmap, uint64_t size);
    void init_after_paging();
private:
    void mark(uint64_t base, uint64_t size, bool usable);
    static bool bitmap_read_bit(uint64_t pos, uint64_t * bitmap);
    static void bitmap_mark_bit(uint64_t pos, bool b, uint64_t * bitmap);
    static void mark(uint64_t base, uint64_t size, bool usable, uint64_t * bitmap);
    static void mark_single(uint64_t base, bool usable, uint64_t * bitmap);
    uint64_t * bitmap = 0;
    uint64_t bitmap_size = 0;
    const uint64_t non_special_base = 0x100000; // Start from 1 MB
};

extern PFrameAllocator palloc;