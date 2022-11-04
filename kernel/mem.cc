#include "mem.hh"
#include <stdint.h>
#include "utils.hh"
#include "misc.hh"
#include "paging.hh"
#include <kernel/memory.h>
#include <kernel/errors.h>

PFrameAllocator palloc;

ReturnStr<void*> PFrameAllocator::alloc_page()
{
    uint64_t found_page = ERROR_OUT_OF_MEMORY;
    uint64_t page = 0;
    // Find free page
    for (uint64_t i = (non_special_base >> (12+6)); i < bitmap_size; ++i) {
        if (bitmap[i] != 0)
            for (int j = 0; j < 64; ++j) {
                if (bitmap_read_bit(i*64 + j, bitmap)) {
                    found_page = SUCCESS;
                    page = i*64 + j;
                    goto skip;
                }
            }
    }
skip:
    if (found_page == SUCCESS) {
        bitmap_mark_bit(page, false, bitmap);
        page <<=12;
    }

    return {found_page, (void*)page};
}

ReturnStr<uint64_t> PFrameAllocator::alloc_page_ppn()
{
    ReturnStr<void*> r = alloc_page();
    return {r.result, (uint64_t)((int64_t)r.val >> 12)};
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
    mark_single((uint64_t)page, true, bitmap);
}

void PFrameAllocator::bitmap_mark_bit(uint64_t pos, bool b, uint64_t * bitmap)
{
    uint64_t l = pos%64;
    uint64_t i = pos/64;

    if (b) bitmap[i] |= (((uint64_t)0x01) << l);
    else bitmap[i] &= ~(((uint64_t)0x01) << l);
}

bool PFrameAllocator::bitmap_read_bit(uint64_t pos, uint64_t * bitmap)
{
    uint64_t l = pos%64;
    uint64_t i = pos/64;

    return (bitmap[i] & ((uint64_t)0x01 << l)) != (uint64_t)0;
}

void PFrameAllocator::init(uint64_t * bitmap, uint64_t size)
{
    this->bitmap = bitmap;
    this->bitmap_size = size;
}

void PFrameAllocator::init_after_paging()
{
    void* addr = unoccupied;
    unoccupied = (void*)((uint64_t)unoccupied + this->bitmap_size_pages());

    Page_Table_Argumments pta = {1, 0, 0, 1, 0};
    for (uint64_t i = 0; i < bitmap_size_pages(); i += 4096) {
        uint64_t phys = phys_addr_of((uint64_t)bitmap + i).val;
        map(phys, (uint64_t)addr + i, pta);
    }

    bitmap = (uint64_t*)addr;
}

uint64_t PFrameAllocator::bitmap_size_pages() const
{
    return bitmap_size*sizeof(uint64_t);
}
