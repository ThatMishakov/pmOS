#include "palloc.hh"
#include "asm.hh"
#include "linker.hh"
#include "common/memory.h"
#include "paging.hh"
#include "types.hh"
#include "utils.hh"

void* palloc_c(size_t number)
{
    void* pages = palloc(number);
    if (pages == nullptr) return pages;
    for (size_t i = 0; i < number; ++i) {
        page_clear((void*)((char*)pages + i*0x1000));
    }
    return pages;
}

palloc_list palloc_head = {0,0};
uint64_t from = (uint64_t)&_palloc_start;
DECLARE_LOCK(palloc_l);

void* palloc(size_t size)
{
    // Spinlock to prevent concurrent accesses
    LOCK(palloc_l)
    palloc_list* l = &palloc_head;
    while (l->next != 0 and l->next->number_pages < size) {
        l = l->next;
    }

    // Variable to be returned
    palloc_list* block;

    // No block of suitable size
    if (l->next == nullptr) {
        // Allocate blocks
        uint64_t p_head = from;
        uint64_t size_p = size * 4096;
        from += size_p;

        // TODO: check page frame allocator errors
        Page_Table_Argumments arg;
        arg.user_access = 0;
        arg.writeable = 1;
        arg.execution_disabled = 1;

        for (uint64_t i = 0; i < size_p; i += 4096) {
            get_page(p_head + i, arg);
        }

        block = (palloc_list*)p_head;
    } else if (l->next->number_pages == size) { // Block of just the right size
        // Remove the block from list and return its base address
        block = l->next;
        l->next = l->next->next;
    } else { // Block of larger size than needed
        // Adjust size in pages to bytes
        uint64_t size_p = size * 4096;

        // Create new block
        palloc_list* new_block = (palloc_list*)((char*)l->next + size_p);
        new_block->number_pages = l->next->number_pages - size;

        // Save the interesting block
        block = l->next;

        // Link new block instead of old one
        new_block->next = l->next->next;
        l->next = new_block;
    }

    UNLOCK(palloc_l)
    return (void*)block;
}

int pfree(void* start, size_t number)
{
    // Spinlock
    LOCK(palloc_l)

    // Create a list palloc_head entry
    palloc_list* page = (palloc_list*)start;
    page->number_pages = number;

    // Insert into the list
    palloc_list* p = &palloc_head; // palloc_head should always be lower in memory than allocator

    // Find a position to insert
    while (p->next != nullptr and p->next < page) {
        p->next = p->next->next;
    }

    // Insert
    page->next = p->next;
    p->next = page;

    // Check if next page can be merged
    if (page->next == (palloc_list*)((char*)page + number*4096)) {
        // Merge entries
        page->number_pages += page->next->number_pages;
        page->next = page->next->next;
    }

    UNLOCK(palloc_l)
    return 0;
}