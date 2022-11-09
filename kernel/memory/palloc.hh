#pragma once
#include <stddef.h>
#include "types.hh"

// Page frame allocator

// Allocates *number* of pages
void* palloc(size_t number);
// Allocates *number* of pages and clears them
void* palloc_c(size_t number);


// Frees pages. Return 0 if successful
int pfree(void* start, size_t number);

// Internal sorted single linked list structure
struct palloc_list {
    palloc_list* next = nullptr;
    size_t number_pages = 0;
};


