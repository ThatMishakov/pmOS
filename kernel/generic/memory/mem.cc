/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mem.hh"
#include <utils.hh>
#include <misc.hh>
#include "paging.hh"
#include <kernel/memory.h>
#include <kernel/errors.h>

PFrameAllocator kernel_pframe_allocator;

void* PFrameAllocator::alloc_page()
{
    u64 found_page = ERROR_OUT_OF_MEMORY;
    u64 page = 0;

    Auto_Lock_Scope scope_lock(lock);

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


    if (found_page != SUCCESS)
        throw(Kern_Exception(found_page, "alloc_page no free frames"));

    bitmap_mark_bit(page, false, bitmap);
    page <<=12;

    return (void*)page;
}

u64 PFrameAllocator::alloc_page_ppn()
{
    void* r = alloc_page();
    return ((u64)r) >> 12;
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

u64 PFrameAllocator::bitmap_size_pages() const
{
    return bitmap_size*sizeof(u64);
}
