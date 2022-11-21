#include "mem.hh"
#include <utils.hh>
#include <misc.hh>
#include "paging.hh"
#include <kernel/memory.h>
#include <kernel/errors.h>

PFrameAllocator palloc;

ReturnStr<void*> PFrameAllocator::alloc_page()
{
    u64 found_page = ERROR_OUT_OF_MEMORY;
    u64 page = 0;

    lock.lock();

    // Find free page
    for (u64 i = smallest; i < bitmap_size; ++i) {
        if (bitmap[i] != 0)
            for (int j = 0; j < 64; ++j) {
                if (bitmap_read_bit(i*64 + j, bitmap)) {
                    smallest = i;
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

    lock.unlock();

    return {found_page, (void*)page};
}

ReturnStr<u64> PFrameAllocator::alloc_page_ppn()
{
    ReturnStr<void*> r = alloc_page();
    return {r.result, (u64)((int64_t)r.val >> 12)};
}

void PFrameAllocator::mark(u64 base, u64 size, bool usable, u64 * bitmap)
{
    u64 base_page = base >> 12;
    u64 size_page = size >> 12;
    if (size_page < 64) {
        for (u64 i = 0; i < size_page; ++i) {
            bitmap_mark_bit(base_page + i, usable, bitmap);
        }
    } else { // TODO: INEFFICIENT!
        u64 pattern = usable ? ~0x0 : 0x0;
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

void PFrameAllocator::mark_single(u64 base, bool usable, u64 * bitmap)
{
    bitmap_mark_bit(base >> 12, usable, bitmap);
}

void PFrameAllocator::free(void* page)
{
    lock.lock();
    u64 smallest_p = (u64)page >> 12 >> 6;
    if (smallest_p < smallest) smallest = smallest_p;
    mark_single((u64)page, true, bitmap);
    lock.unlock();
}

void PFrameAllocator::bitmap_mark_bit(u64 pos, bool b, u64 * bitmap)
{
    u64 l = pos%64;
    u64 i = pos/64;

    if (b) bitmap[i] |= (((u64)0x01) << l);
    else bitmap[i] &= ~(((u64)0x01) << l);
}

bool PFrameAllocator::bitmap_read_bit(u64 pos, u64 * bitmap)
{
    u64 l = pos%64;
    u64 i = pos/64;

    return (bitmap[i] & ((u64)0x01 << l)) != (u64)0;
}

void PFrameAllocator::init(u64 * bitmap, u64 size)
{
    this->bitmap = bitmap;
    this->bitmap_size = size;
}

void PFrameAllocator::init_after_paging()
{
    void* addr = unoccupied;
    unoccupied = (void*)((u64)unoccupied + this->bitmap_size_pages());

    Page_Table_Argumments pta = {1, 0, 0, 1, 0};
    for (u64 i = 0; i < bitmap_size_pages(); i += 4096) {
        u64 phys = phys_addr_of((u64)bitmap + i).val;
        map(phys, (u64)addr + i, pta);
    }

    bitmap = (u64*)addr;
}

u64 PFrameAllocator::bitmap_size_pages() const
{
    return bitmap_size*sizeof(u64);
}
