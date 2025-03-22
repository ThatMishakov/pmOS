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

#pragma once

// !!!!!! HUGE TODO !!!!!!!!!!!
// Investigate locks and concurrency in the allocator

/** Kernel's virtual memory allocator
 *
 * virtmem/vmm is a virtual memory allocator for the kernel. It is inspired by vmem algorithm
 * created by Bonwick and Adams but it currently is a very simple implementation since having a more
 * complete and efficient one is not a priority at the moment.
 */

#include <assert.h>
#include <errno.h>
#include <pmos/containers/intrusive_list.hh>
#include <types.hh>

#include <kern_logger/kern_logger.hh>

namespace kernel::vmm
{

struct VirtmemBoundaryTag {
    // Either a double linked list of free sigments of a given size
    // or a links of the hash table of allocated segments
    pmos::containers::DoubleListHead<VirtmemBoundaryTag> ll = {};

    // Base address of the segment
    u64 base = 0;
    // Size of the segment
    u64 size = 0;

    // Doubly linked list of all segments, sorted by address, or a doubly linked list of free
    // boundary tags
    VirtmemBoundaryTag *segment_next = nullptr;
    VirtmemBoundaryTag *segment_prev = nullptr;
    // Segment end address
    constexpr u64 end() const { return base + size; }

    enum class State : u8 {
        FREE = 0,
        ALLOCATED,
        LISTHEAD, // The segment is a head of the list of all segments
    } state = State::FREE;
    // Defines if the boundary tag is used

    constexpr bool has_prev() const { return segment_prev != nullptr; }
    constexpr bool has_next() const { return segment_next != nullptr; }
};

// Use circular lists for both free segments and allocated segments (hash table)
// This is prefered because of the fast removal and addition of elements
//
// The initializer must be called manually, since the list must point to itself, and the address
// might not be known at compile time (on PIE executables). While this could be done by C++
// global constructors, the vmm might be initialized before they are called.
using VirtMemFreelist =
    pmos::containers::CircularDoubleList<VirtmemBoundaryTag, &VirtmemBoundaryTag::ll>;

static const u64 virtmem_initial_segments = 16;
// Static array of initial boundary tags, used during the initialization of the kernel
// This is done this way as the new tags are then allocated using the allocator itself,
// which would not be available during the initialization, creating a chicken-egg problem.
// The next allocations will then work fine, as long as there is 1 free segment in the
// freelist.
inline VirtmemBoundaryTag virtmem_initial_tags[virtmem_initial_segments];

// The unsorted list of unused boundary tags
constinit inline VirtMemFreelist virtmem_available_tags_list;
inline u64 virtmem_available_tags_count = 0;

// Get an unused boundary tag from the list
VirtmemBoundaryTag *virtmem_get_free_tag();
// Return an unused boundary tag to the list
void virtmem_return_tag(VirtmemBoundaryTag *tag);

// Makes sure that enough boundary tags are available
// Returns SUCCESS (0) if the operation was successful or an error code otherwise
int virtmem_ensure_tags(size_t size);

// This function adds the initial tags to the freelist and shall be called during the initialization
// of the allocator during the kernel boot.
void virtmem_fill_initial_tags();

// Initialize the allocator during booting, and add the space designated for the allocator to the
// freelist
void virtmem_init(u64 virtmem_base, u64 virtmem_size);

template<int QUANTUM, int MAX_ORDER> class VirtMem
{
public:
    enum class VirtmemAllocPolicy {
        INSTANTFIT,
        BESTFIT,
    };

    // Allocate a segment of a given size, without any specific alignment
    // This function does not fill the segment with memory pages, if that is desired, the page
    // frame allocator should be invoked separately.
    // Returns the base address of the segment or nullptr if the allocation failed (when the system
    // is out of memory)
    void *virtmem_alloc(u64 npages, VirtmemAllocPolicy policy = VirtmemAllocPolicy::INSTANTFIT);

    // Allocate a segment of a given size, with a specific alignment
    // This function is similar to virtmem_alloc, but it also takes the alignment into account.
    // The alignment is a log2 of the number of pages, so the segment will be aligned to 2^alignment
    void *virtmem_alloc_aligned(u64 npages, u64 alignment);

    // Free a segment. Only complete segments can be freed and the size is used as a sanity check.
    void virtmem_free(void *ptr, u64 npages);

    // Initializes a free freelist
    void init();

protected:
    // Page size in bytes
    static const u64 freelist_quantum   = QUANTUM;
    // Max log2 of boundary tag size
    static const u64 freelist_max_order = MAX_ORDER;
    // Number of entries in array of free lists
    static const u64 freelist_count     = freelist_max_order - freelist_quantum + 1;
    // Bitmap indicating which sizes of free lists are non-empty
    u64 virtmem_freelist_bitmap         = 0;

    // Array of free lists
    VirtMemFreelist virtmem_freelists[freelist_count];

    // TODO: This probably shouldn't be here...
    static int find_bit(u64 t)
    {
#ifndef __i386__
        return sizeof(t) * 8 - __builtin_clzl(t);
#else
        return sizeof(t) * 8 - __builtin_clzll(t);
#endif
    }
    static int first_bit(u64 t)
    {
#ifndef __i386__
        return __builtin_ffsl(t);
#else
        return __builtin_ffsll(t);
#endif
    }

    // Find the index of the freelists array corresponding to the given size
    int virtmem_freelist_index(u64 size)
    {
        if (size < (1UL << freelist_quantum))
            return 0;

        u64 l = find_bit(size);
        return l - freelist_quantum;
    }

    static const u64 virtmem_initial_hash_size = 16;
    // Initial size of the hash table of allocated segments. After the initialization, the hash
    // table can then resize itself as needed, switching to different dynamically allocated array.
    // The hash table array is always a power of 2, so the mask can be used instead of modulo.
    VirtMemFreelist virtmem_initial_hash[virtmem_initial_hash_size];

    u64 virtmem_hashtable_entries      = 0;
    VirtMemFreelist *virtmem_hashtable = nullptr;
    u64 virtmem_hashtable_size         = virtmem_initial_hash_size;
    u64 virtmem_hashtable_size_bytes() { return virtmem_hashtable_size * sizeof(VirtMemFreelist); }
    u64 virtmem_hash_mask() { return virtmem_hashtable_size - 1; }

    // Save the tag to the hash table. This function will try to optimistically resize the hash
    // table, but might postpone it if the system is under memory pressure and the allocation fails.
    void virtmem_save_to_alloc_hashtable(VirtmemBoundaryTag *tag);

    // Adds a boundary tag to the appropriate list of free segments
    void virtmem_add_to_free_list(VirtmemBoundaryTag *tag);

    constexpr u64 virtmem_hashtable_index(u64 base)
    {
        // Hashtable size is always a power of 2
        u64 mask = virtmem_hashtable_size - 1;

        // TODO: Add a hash function here
        return (base >> 12) & mask;
    }

    // Dummy head of the list of segments sorted by address
    // This is used to simplify the code and avoid special cases when adding new segments
    VirtmemBoundaryTag segment_ll_dummy_head;

    friend int virtmem_ensure_tags(size_t size);
    friend void virtmem_init(u64 virtmem_base, u64 virtmem_size);
};

inline constinit VirtMem<12, 63> kernel_space_allocator;

// Links the boundary tag with the list of tags sorted by address
void virtmem_link_tag(VirtmemBoundaryTag *preceeding, VirtmemBoundaryTag *new_tag);
// Unlinks the boundary tag from the list of tags sorted by address
void virtmem_unlink_tag(VirtmemBoundaryTag *tag);

template<int Q, int M> void VirtMem<Q, M>::virtmem_free(void *ptr, u64 npages)
{
    // Find the tag that corresponds to the base of the segment
    u64 base                = (u64)ptr;
    u64 idx                 = virtmem_hashtable_index(base);
    auto head               = &virtmem_hashtable[idx];
    VirtmemBoundaryTag *tag = nullptr;
    for (auto i = head->begin(); i != head->end(); i++) {
        if (i->base == base) {
            // Found the tag
            tag = i;
            break;
        }
    }

    assert(tag != nullptr);
    assert(tag->state == VirtmemBoundaryTag::State::ALLOCATED);
    assert(tag->size == (npages << freelist_quantum));

    // Take the tag out of the hashtable
    VirtMemFreelist::remove(tag);

    // Mark the tag as free
    tag->state = VirtmemBoundaryTag::State::FREE;

    // Merge the tag with the previous and next tags if they are free
    if (tag->segment_prev->state == VirtmemBoundaryTag::State::FREE) {
        auto prev = tag->segment_prev;
        VirtMemFreelist::remove(prev);
        tag->base = prev->base;
        tag->size += prev->size;
        virtmem_unlink_tag(prev);
        virtmem_return_tag(prev);
    }

    if (tag->segment_next->state == VirtmemBoundaryTag::State::FREE) {
        auto next = tag->segment_next;
        VirtMemFreelist::remove(next);
        tag->size += next->size;
        virtmem_unlink_tag(next);
        virtmem_return_tag(next);
    }

    // Add the tag to the free list
    virtmem_add_to_free_list(tag);
}

template<int Q, int M> void VirtMem<Q, M>::virtmem_save_to_alloc_hashtable(VirtmemBoundaryTag *tag)
{
    // This function is also supposed to resize the hashtable if needed.
    // But for now, it is not a priority and the the performance hit is
    // not a concern compared to other parts of the kernel.

    u64 idx = virtmem_hashtable_index(tag->base);
    virtmem_hashtable[idx].push_front(tag);
}

template<int Q, int M> void *VirtMem<Q, M>::virtmem_alloc_aligned(u64 npages, u64 alignment)
{
    // I don't anticipate this function to be used frequently, so just implement best fit for now

    // Make sure there are 2 boundary tags, as this operation might split a segment in 3
    if (virtmem_ensure_tags(2) != 0)
        return nullptr;

    const u64 alignup_mask = (1UL << alignment) - 1;

    // Calculate the size of the segment in bytes
    const u64 size = npages << 12;

    // Find the smallest list that can fit the requested size
    int l = find_bit(npages);

    VirtmemBoundaryTag *t = nullptr;
    u64 mask              = ~((1ULL << l) - 1);
    u64 wanted_mask       = mask & virtmem_freelist_bitmap;
    while (wanted_mask != 0) {
        // __builtin_ffsl returns the index of the first set bit + 1, or 0 if there are none
        int idx    = first_bit(wanted_mask) - 1;
        auto &list = virtmem_freelists[idx];
        for (auto tag = list.begin(); tag != list.end(); tag++) {
            u64 base    = tag->base;
            u64 alignup = (base + alignup_mask) & ~alignup_mask;
            u64 all_end = alignup + size;
            if (all_end <= tag->end()) {
                t = tag;
                goto found;
            }
        }

        mask <<= 1;
        wanted_mask = mask & virtmem_freelist_bitmap;
    }

    t_print_bochs("No segment of wanted size and alignment is available\n");

    // No segment of wanted size and alignment is available
    return nullptr;

found:
    const u64 aligned_up_base = (t->base + alignup_mask) & ~alignup_mask;
    const u64 old_base        = t->base;

    t->base = aligned_up_base;
    t->size -= aligned_up_base - old_base;
    t->state = VirtmemBoundaryTag::State::ALLOCATED;

    if ((old_base & alignup_mask) != 0) {
        // The segment base is misaligned, split the segment
        auto new_tag   = virtmem_get_free_tag();
        new_tag->base  = old_base;
        new_tag->size  = aligned_up_base - old_base;
        new_tag->state = VirtmemBoundaryTag::State::FREE;
        virtmem_link_tag(t->segment_prev, new_tag);
        virtmem_add_to_free_list(new_tag);
    }

    if (t->end() > aligned_up_base + size) {
        // The segment is larger than the requested size
        // Split the segment and return the first part
        auto new_tag   = virtmem_get_free_tag();
        new_tag->base  = aligned_up_base + size;
        new_tag->size  = t->end() - (aligned_up_base + size);
        new_tag->state = VirtmemBoundaryTag::State::FREE;
        virtmem_link_tag(t, new_tag);
        virtmem_add_to_free_list(new_tag);
    }

    virtmem_save_to_alloc_hashtable(t);
    return (void *)aligned_up_base;
}

template<int Q, int M> void *VirtMem<Q, M>::virtmem_alloc(u64 npages, VirtmemAllocPolicy policy)
{
    // Make sure there is 1 free boundary tag available, in case the segment needs to be split
    if (virtmem_ensure_tags(1) != 0)
        return nullptr;

    assert(npages > 0);

    // Calculate the size of the segment in bytes
    u64 size = npages << 12;

    // Find the smallest list that can fit the requested size
    int l = find_bit(npages);

    VirtmemBoundaryTag *best = nullptr;
    u64 free_list_idx        = 0;
    if (policy == VirtmemAllocPolicy::BESTFIT and (1UL << l) < npages) {
        // The requested size is not a power of 2 and best fit is requested
        // This implies searching the lists for the best fit
        // Before going to larger ones
        auto &list    = virtmem_freelists[l];
        free_list_idx = l;
        for (auto tag = list.begin(); tag != list.end(); tag++) {
            if (tag->size == npages) {
                // The exact fit has been found. Use it directly
                VirtMemFreelist::remove(tag);
                if (list.empty())
                    // Mark the list as empty
                    virtmem_freelist_bitmap &= ~(1UL << l);

                tag->state = VirtmemBoundaryTag::State::ALLOCATED;
                virtmem_save_to_alloc_hashtable(tag);
                return (void *)tag->base;
            }

            if (tag->size > npages and (best == nullptr or tag->size < best->size))
                best = tag;
        }
    }

    if (best == nullptr) {
        // The best fit hasn't been wanted or found. Go to the first fit of some larger list
        // which is guaranteed to have a fit

        // Find the first non-empty list on bitmap
        u64 mask = (1UL << (l + 1)) - 1;
        mask     = ~mask;

        u64 wanted_mask = mask & virtmem_freelist_bitmap;
        if (wanted_mask == 0)
            // No segment of the requested size is available
            return nullptr;

        // __builtin_ffsl returns the index of the first set bit + 1, or 0 if there are none
        int idx       = first_bit(wanted_mask) - 1;
        auto &list    = virtmem_freelists[idx];
        best          = &list.front();
        free_list_idx = idx;
    }

    if (best->size == size) {
        // The segment is an exact fit
        VirtMemFreelist::remove(best);
        auto &list = virtmem_freelists[free_list_idx];
        if (list.empty())
            // Mark the list as empty
            virtmem_freelist_bitmap &= ~(1UL << free_list_idx);

        best->state = VirtmemBoundaryTag::State::ALLOCATED;
        virtmem_save_to_alloc_hashtable(best);
        return (void *)best->base;
    } else {
        // The segment is larger than the requested size
        // Split the segment and return the first part
        auto new_tag   = virtmem_get_free_tag();
        new_tag->base  = best->base + size;
        new_tag->size  = best->size - size;
        new_tag->state = VirtmemBoundaryTag::State::FREE;

        best->size  = size;
        best->state = VirtmemBoundaryTag::State::ALLOCATED;
        virtmem_link_tag(best, new_tag);
        virtmem_save_to_alloc_hashtable(best);
        virtmem_add_to_free_list(new_tag);
        return (void *)best->base;
    }
}

template<int Q, int M> void VirtMem<Q, M>::virtmem_add_to_free_list(VirtmemBoundaryTag *tag)
{
    int idx    = virtmem_freelist_index(tag->size);
    auto &list = virtmem_freelists[idx];
    list.push_front(tag);
    virtmem_freelist_bitmap |= (1UL << idx);
}

template<int Q, int M> void VirtMem<Q, M>::init()
{
    segment_ll_dummy_head.segment_next = &segment_ll_dummy_head;
    segment_ll_dummy_head.segment_prev = &segment_ll_dummy_head;
    segment_ll_dummy_head.state        = VirtmemBoundaryTag::State::LISTHEAD;

    // // Init lists
    // // See VirtMemFreelist::init_empty() for the explanation
    // for (auto &i: virtmem_freelists)
    //     i.init();
    // for (auto &i: virtmem_initial_hash)
    //     i.init();
    virtmem_hashtable = virtmem_initial_hash;
}

}; // namespace kernel::vmm