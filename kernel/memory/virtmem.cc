#include "virtmem.hh"
#include <kernel/errors.h>
#include <assert.h>

void virtmem_fill_initial_tags() {
    for (u64 i = 0; i < virtmem_initial_segments; ++i) {
        virtmem_return_tag(&virtmem_initial_tags[i]);
    }
}

void virtmem_add_to_list(VirtMemFreelist* list, VirtmemBoundaryTag* tag) {
    tag->ll_next = list->ll_next;
    tag->ll_prev = (VirtmemBoundaryTag*)list;
    list->ll_next->ll_prev = tag;
    list->ll_next = tag;
}

void virtmem_remove_from_list(VirtmemBoundaryTag* tag) {
    tag->ll_prev->ll_next = tag->ll_next;
    tag->ll_next->ll_prev = tag->ll_prev;
}

VirtmemBoundaryTag *virtmem_get_free_tag() {
    if (virtmem_available_tags_list.is_empty()) 
        return nullptr;

    VirtmemBoundaryTag* tag = virtmem_available_tags_list.ll_next;
    virtmem_remove_from_list(tag);
    virtmem_available_tags_count--;
    return tag;
}

void virtmem_return_tag(VirtmemBoundaryTag* tag) {
    virtmem_add_to_list(&virtmem_available_tags_list, tag);
    virtmem_available_tags_count++;
}

void virtmem_init(u64 virtmem_base, u64 virtmem_size) {
    segment_ll_dummy_head.segment_next = &segment_ll_dummy_head;
    segment_ll_dummy_head.segment_prev = &segment_ll_dummy_head;
    segment_ll_dummy_head.state = VirtmemBoundaryTag::State::LISTHEAD;

    // Init lists
    // See VirtMemFreelist::init_empty() for the explanation
    for (auto& i : virtmem_freelists)
        i.init_empty();
    for (auto& i : virtmem_initial_hash)
        i.init_empty();
    virtmem_hashtable = virtmem_initial_hash;
    virtmem_available_tags_list.init_empty();

    virtmem_fill_initial_tags();
    auto tag = virtmem_get_free_tag();
    tag->base = virtmem_base;
    tag->size = virtmem_size;
    tag->state = VirtmemBoundaryTag::State::FREE;
    virtmem_add_to_free_list(tag);
    virtmem_link_tag(&segment_ll_dummy_head, tag);
}

void virtmem_add_to_free_list(VirtmemBoundaryTag* tag) {
    int idx = virtmem_freelist_index(tag->size);
    auto &list = virtmem_freelists[idx];
    virtmem_add_to_list(&list, tag);
    virtmem_freelist_bitmap |= (1 << idx);
}

u64 virtmem_ensure_tags(u64 size) {
    // TODO: Implement this function
    // For now, just check if the tags are available and error otherwise
    if (virtmem_available_tags_count >= size)
        return SUCCESS;

    // -- The algoritm for allocating new tags would go here --
    return ERROR_OUT_OF_MEMORY;
}

int virtmem_freelist_index(u64 size) {
    if (size < 4096)
        return 0;
    
    u64 l = sizeof(size)*8 - __builtin_clzl(size);
    return l - freelist_quantum;
}

void *virtmem_alloc(u64 npages, VirtmemAllocPolicy policy) {
    // Make sure there is 1 free boundary tag available, in case the segment needs to be split
    if (virtmem_ensure_tags(1) != SUCCESS)
        return nullptr;

    // Calculate the size of the segment in bytes
    u64 size = npages << 12;

    // Find the smallest list that can fit the requested size
    int l = sizeof(npages)*8 - __builtin_clzl(npages);

    VirtmemBoundaryTag *best = nullptr;
    u64 free_list_idx = 0;
    if (policy == VirtmemAllocPolicy::BESTFIT
        and (1UL << l) < npages) {
        // The requested size is not a power of 2 and best fit is requested
        // This implies searching the lists for the best fit
        // Before going to larger ones
        auto &list = virtmem_freelists[l];
        free_list_idx = l;
        for (auto tag = list.ll_next; tag != (VirtmemBoundaryTag*)&list; tag = tag->ll_next) {
            if (tag->size == npages) {
                // The exact fit has been found. Use it directly
                virtmem_remove_from_list(tag);
                if (list.ll_next == (VirtmemBoundaryTag*)&list)
                    // Mark the list as empty
                    virtmem_freelist_bitmap &= ~(1 << l);

                tag->state = VirtmemBoundaryTag::State::ALLOCATED;
                virtmem_save_to_alloc_hashtable(tag);
                return (void*)tag->base;
            }

            if (tag->size > npages and (best == nullptr or tag->size < best->size))
                best = tag;
        }
    }

    if (best != nullptr) {
        // The best fit hasn't been wanted or found. Go to the first fit of some larger list
        // which is guaranteed to have a fit

        // Find the first non-empty list on bitmap
        u64 mask = (1UL << (l + 1)) - 1;
        mask = ~mask;

        u64 wanted_mask = mask & virtmem_freelist_bitmap;
        if (wanted_mask == 0)
            // No segment of the requested size is available
            return nullptr;
        
        // __builtin_ffsl returns the index of the first set bit + 1, or 0 if there are none
        int idx = __builtin_ffsl(wanted_mask) - 1;
        auto &list = virtmem_freelists[idx];
        best = list.ll_next;
        free_list_idx = idx;
    }

    if (best->size == size) {
        // The segment is an exact fit
        virtmem_remove_from_list(best);
        auto &list = virtmem_freelists[free_list_idx];
        if (list.ll_next == (VirtmemBoundaryTag*)&list)
            // Mark the list as empty
            virtmem_freelist_bitmap &= ~(1 << free_list_idx);

        best->state = VirtmemBoundaryTag::State::ALLOCATED;
        virtmem_save_to_alloc_hashtable(best);
        return (void*)best->base;
    } else {
        // The segment is larger than the requested size
        // Split the segment and return the first part
        auto new_tag = virtmem_get_free_tag();
        new_tag->base = best->base + size;
        new_tag->size = best->size - size;
        new_tag->state = VirtmemBoundaryTag::State::FREE;

        best->size = size;
        best->state = VirtmemBoundaryTag::State::ALLOCATED;
        virtmem_link_tag(best, new_tag);
        virtmem_save_to_alloc_hashtable(best);
        virtmem_add_to_free_list(new_tag);
        return (void*)best->base;
    }
}

void *virtmem_alloc_aligned(u64 npages, u64 alignment) {
    // I don't anticipate this function to be used frequently, so just implement best fit for now

    // Make sure there are 2 boundary tags, as this operation might split a segment in 3
    if (virtmem_ensure_tags(2) != SUCCESS)
        return nullptr;

    const u64 alignup_mask = (1UL << alignment) - 1;

    // Calculate the size of the segment in bytes
    const u64 size = npages << 12;

    // Find the smallest list that can fit the requested size
    int l = sizeof(npages) - __builtin_clzl(npages);

    VirtmemBoundaryTag *t = nullptr;
    u64 mask = (1 << l) - 1;
    u64 wanted_mask = mask & virtmem_freelist_bitmap;
    while (wanted_mask != 0) {
        // __builtin_ffsl returns the index of the first set bit + 1, or 0 if there are none
        int idx = __builtin_ffsl(wanted_mask) - 1;
        auto &list = virtmem_freelists[idx];
        for (auto tag = list.ll_next; tag != (VirtmemBoundaryTag*)&list; tag = tag->ll_next) {
            u64 base = tag->base;
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

    // No segment of wanted size and alignment is available
    return nullptr;

found:
    const u64 aligned_up_base = (t->base + alignup_mask) & ~alignup_mask;
    const u64 old_base = t->base;

    t->base = aligned_up_base;
    t->size -= aligned_up_base - old_base;
    t->state = VirtmemBoundaryTag::State::ALLOCATED;

    if ((old_base & alignup_mask) != 0) {
        // The segment base is misaligned, split the segment
        auto new_tag = virtmem_get_free_tag();
        new_tag->base = old_base;
        new_tag->size = aligned_up_base - old_base;
        new_tag->state = VirtmemBoundaryTag::State::FREE;
        virtmem_link_tag(t->segment_prev, new_tag);
        virtmem_add_to_free_list(new_tag);
    }

    if (t->end() > aligned_up_base + size) {
        // The segment is larger than the requested size
        // Split the segment and return the first part
        auto new_tag = virtmem_get_free_tag();
        new_tag->base = aligned_up_base + size;
        new_tag->size = t->end() - (aligned_up_base + size);
        new_tag->state = VirtmemBoundaryTag::State::FREE;
        virtmem_link_tag(t, new_tag);
        virtmem_add_to_free_list(new_tag);
    }
    
    virtmem_save_to_alloc_hashtable(t);
    return (void*)aligned_up_base;
}

void virtmem_link_tag(VirtmemBoundaryTag* prev, VirtmemBoundaryTag* next) {
    next->segment_prev = prev;
    next->segment_next = prev->segment_next;
    prev->segment_next->segment_prev = next;
    prev->segment_next = next;
}

u64 virtmem_hashtable_index(u64 base) {
    // Hashtable size is always a power of 2
    u64 mask = virtmem_hashtable_size - 1;

    // TODO: Add a hash function here
    return (base >> 12) & mask;
}

void virtmem_save_to_alloc_hashtable(VirtmemBoundaryTag* tag) {
    // This function is also supposed to resize the hashtable if needed.
    // But for now, it is not a priority and the the performance hit is
    // not a concern compared to other parts of the kernel.

    u64 idx = virtmem_hashtable_index(tag->base);
    auto head = &virtmem_hashtable[idx];
    virtmem_add_to_list(head, tag);
}

void virtmem_free(void *ptr, u64 npages) {
    // Find the tag that corresponds to the base of the segment
    u64 base = (u64)ptr;
    u64 idx = virtmem_hashtable_index(base);
    auto head = &virtmem_hashtable[idx];
    VirtmemBoundaryTag *tag = nullptr;
    for (auto i = head->ll_next; i != (VirtmemBoundaryTag*)head; i = i->ll_next) {
        if (tag->base == base) {
            // Found the tag
            tag = i;
            break;
        }
    }

    assert(tag != nullptr);
    assert(tag->state == VirtmemBoundaryTag::State::ALLOCATED);
    assert(tag->size == npages);

    // Take the tag out of the hashtable
    virtmem_remove_from_list(tag);

    // Mark the tag as free
    tag->state = VirtmemBoundaryTag::State::FREE;

    // Merge the tag with the previous and next tags if they are free
    if (tag->segment_prev->state == VirtmemBoundaryTag::State::FREE) {
        auto prev = tag->segment_prev;
        virtmem_remove_from_list(prev);
        tag->base = prev->base;
        tag->size += prev->size;
        virtmem_unlink_tag(prev);
        virtmem_return_tag(prev);
    }

    if (tag->segment_next->state == VirtmemBoundaryTag::State::FREE) {
        auto next = tag->segment_next;
        virtmem_remove_from_list(next);
        tag->size += next->size;
        virtmem_unlink_tag(next);
        virtmem_return_tag(next);
    }

    // Add the tag to the free list
    virtmem_add_to_free_list(tag);
}
