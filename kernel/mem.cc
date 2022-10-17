#include "mem.hh"
#include <stdint.h>
#include "utils.hh"
#include "misc.hh"
#include "paging.hh"

PFrameAllocator palloc;

void* PFrameAllocator::alloc_page()
{
    uint64_t page = -1;
    // Find free page
    for (uint64_t i = non_special_base >> (12+6); i < bitmap_size; ++i) {
        if (bitmap[i])
            for (int j = 0; j < 64; ++j) {
                if (bitmap_read_bit(i*64 + j, bitmap)) {
                    page = i*64 + j;
                    goto skip;
                }
            }
    }
skip:
    bitmap_mark_bit(page, false, bitmap);
    if (page != -1) page <<=12;

    return (void*)page;
}

uint64_t PFrameAllocator::alloc_page_ppn()
{
    return (uint64_t)((int64_t)alloc_page() >> 12);
}

void PFrameAllocator::mark(uint64_t base, uint64_t size, bool usable, uint64_t * bitmap)
{
    uint64_t base_page = base >> 12;
    uint64_t size_page = size >> 12;
    if (size_page < 64) {
        for (uint64_t i = 0; i < size_page; ++i) {
            bitmap_mark_bit(base_page + i, usable, bitmap);
        }
    } else { // TODO: INEFFICIENT!
        uint64_t pattern = usable ? ~0x0 : 0x0;
        while (base_page & 0x3f && size_page > 0) {
            bitmap_mark_bit(base_page, usable, bitmap);
            ++base_page;
            --size_page;
        }
        while (size_page > 64) {
            bitmap[base_page >> 6] = pattern;
            base_page += 64;
            size_page -= 64;
        }
        while (size_page > 0) {
            bitmap_mark_bit(base_page, usable, bitmap);
            ++base_page;
            --size_page;
        }
    }
}

void PFrameAllocator::mark_single(uint64_t base, bool usable, uint64_t * bitmap)
{
    bitmap_mark_bit(base >> 12, usable, bitmap);
}

void PFrameAllocator::free(void* page)
{
    t_print("Debug: Freed page %h\n", page);
    mark_single((uint64_t)page, true, bitmap);
}

void PFrameAllocator::bitmap_mark_bit(uint64_t pos, bool b, uint64_t * bitmap)
{
    uint64_t l = pos & 0x3f;
    uint64_t i = pos >> 6;

    if (b) bitmap[i] |= 0x01 << l;
    else bitmap[i] &= ~(0x01 << l);
}

bool PFrameAllocator::bitmap_read_bit(uint64_t pos, uint64_t * bitmap)
{
    uint64_t l = pos & 0x3f;
    uint64_t i = pos >> 6;

    return !!(bitmap[i] & (0x01 << l));
}

void PFrameAllocator::init(uint64_t * bitmap, uint64_t size)
{
    this->bitmap = bitmap;
    this->bitmap_size = size;
}

void PFrameAllocator::init_after_paging()
{
    void* addr = unoccupied;
    unoccupied = (void*)((uint64_t)unoccupied + this->bitmap_size);

    Page_Table_Argumments pta = {1, 0, 0, 1, 0};
    for (uint64_t i = 0; i < bitmap_size; i += 4096) {
        map((uint64_t)bitmap + i, (uint64_t)addr + i, pta);
    }

    bitmap = (uint64_t*)addr;
    t_print("Debug: Mapped the bitmap to kernel. Location: %h\n", bitmap);
}
