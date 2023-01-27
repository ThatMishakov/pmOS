#pragma once
#include <types.hh>

class PFrameAllocator {
public:
    void free(void* page);
    inline void free_ppn(u64 ppn)
    {
        return free((void*)(ppn << 12));
    }

    // Returns -1 if not found
    ReturnStr<void*> alloc_page();
    ReturnStr<u64> alloc_page_ppn();
    void reserve(void* base, u64 size);

    void init(u64 * bitmap, u64 size);
    void init_after_paging();
    u64 bitmap_size_pages() const;
private:
    Spinlock lock;
    void mark(u64 base, u64 size, bool usable);
    static bool bitmap_read_bit(u64 pos, u64 * bitmap);
    static void bitmap_mark_bit(u64 pos, bool b, u64 * bitmap);
    static void mark(u64 base, u64 size, bool usable, u64 * bitmap);
    static void mark_single(u64 base, bool usable, u64 * bitmap);
    u64 * bitmap = 0;
    u64 bitmap_size = 0;
    const u64 non_special_base = 0x100000; // Start from 1 MB
    u64 smallest = non_special_base >> (12+6);
};

extern PFrameAllocator kernel_pframe_allocator;