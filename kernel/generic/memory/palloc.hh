#pragma once
#include <stddef.h>
#include <types.hh>

// Page frame allocator
// TODO: Now there is virtmem for this

// Allocates *number* of pages
void* palloc(size_t number);
// Allocates *number* of pages and clears them
void* palloc_c(size_t number);


// Frees pages. Return 0 if successful
int pfree(void* start, size_t number);



