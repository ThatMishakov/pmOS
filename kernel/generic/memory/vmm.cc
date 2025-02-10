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

#include "vmm.hh"

#include "paging.hh"
#include "pmm.hh"

#include <assert.h>
#include <errno.h>

using namespace kernel;

namespace kernel::vmm
{

void virtmem_fill_initial_tags()
{
    for (size_t i = 0; i < virtmem_initial_segments; ++i) {
        virtmem_return_tag(&virtmem_initial_tags[i]);
    }
}

VirtmemBoundaryTag *virtmem_get_free_tag()
{
    if (virtmem_available_tags_list.empty())
        return nullptr;

    VirtmemBoundaryTag *tag = &*virtmem_available_tags_list.begin();
    VirtMemFreelist::remove(tag);
    virtmem_available_tags_count--;
    return tag;
}

void virtmem_return_tag(VirtmemBoundaryTag *tag)
{
    virtmem_available_tags_list.push_front(tag);
    virtmem_available_tags_count++;
}

void virtmem_init(u64 virtmem_base, u64 virtmem_size)
{
    kernel_space_allocator.init();

    virtmem_fill_initial_tags();
    auto tag   = virtmem_get_free_tag();
    tag->base  = virtmem_base;
    tag->size  = virtmem_size;
    tag->state = VirtmemBoundaryTag::State::FREE;
    kernel_space_allocator.virtmem_add_to_free_list(tag);
    virtmem_link_tag(&kernel_space_allocator.segment_ll_dummy_head, tag);
}

int virtmem_ensure_tags(size_t size)
{
    if (virtmem_available_tags_count >= size)
        return 0;

    // Allocate a page and slice it into boundary tags
    // Find the smallest tag and take the first page out of it
    if (kernel_space_allocator.virtmem_freelist_bitmap == 0)
        // No free tags
        return -ENOMEM;

    int idx    = kernel_space_allocator.first_bit(kernel_space_allocator.virtmem_freelist_bitmap) - 1;
    auto &list = kernel_space_allocator.virtmem_freelists[idx];
    auto tag   = &*list.begin();

    const auto addr = tag->base;
    phys_addr_t page_phys   = -1UL;

    page_phys = pmm::get_memory_for_kernel(1);
    if (page_phys == -1UL)
        return -ENOMEM;

    Page_Table_Argumments pta = {
        .readable           = true,
        .writeable          = true,
        .user_access        = false,
        .global             = false,
        .execution_disabled = true,
        .extra              = 0,
    };
    auto result = map_kernel_page(page_phys, (void *)addr, pta);
    if (result) {
        pmm::free_memory_for_kernel(page_phys, 1);
        return result;
    }

    // Slice the page into boundary tags
    VirtmemBoundaryTag *new_tags = (VirtmemBoundaryTag *)addr;
    for (size_t i = 0; i < 4096 / sizeof(VirtmemBoundaryTag); ++i) {
        virtmem_return_tag(&new_tags[i]);
    }

    // Add a new boundary tag
    if (tag->size == 4096) {
        // Boundary tag is an exact fit. Take it out of free and mark as used
        VirtMemFreelist::remove(tag);
        if (list.empty())
            // Mark the list as empty
            kernel_space_allocator.virtmem_freelist_bitmap &= ~(1UL << idx);

        tag->state = VirtmemBoundaryTag::State::ALLOCATED;
        kernel_space_allocator.virtmem_save_to_alloc_hashtable(tag);
    } else {
        auto new_tag   = virtmem_get_free_tag();
        new_tag->base  = tag->base;
        new_tag->size  = 4096;
        new_tag->state = VirtmemBoundaryTag::State::ALLOCATED;

        tag->base += 4096;
        tag->size -= 4096;
        virtmem_link_tag(tag->segment_prev, new_tag);
        kernel_space_allocator.virtmem_save_to_alloc_hashtable(new_tag);

        // If the old tag freelist index is different, move it
        int new_idx = kernel_space_allocator.virtmem_freelist_index(tag->size);
        if (new_idx != idx) {
            VirtMemFreelist::remove(tag);
            if (list.empty())
                // Mark the list as empty
                kernel_space_allocator.virtmem_freelist_bitmap &= ~(1UL << idx);

            kernel_space_allocator.virtmem_add_to_free_list(tag);
        }
    }

    // Allocate more in case 1024 isn't enough
    return virtmem_ensure_tags(size);
}

void virtmem_link_tag(VirtmemBoundaryTag *prev, VirtmemBoundaryTag *next)
{
    next->segment_prev               = prev;
    next->segment_next               = prev->segment_next;
    prev->segment_next->segment_prev = next;
    prev->segment_next               = next;
}

void virtmem_unlink_tag(VirtmemBoundaryTag *tag)
{
    tag->segment_prev->segment_next = tag->segment_next;
    tag->segment_next->segment_prev = tag->segment_prev;
}

} // namespace kernel::vmm