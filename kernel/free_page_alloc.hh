#pragma once
#include <stdint.h>
#include "types.hh"

#define FREE_PAGES_SIZE 16

kresult_t free_page_alloc_init();

ReturnStr<uint64_t> get_free_page();

void release_free_page(uint64_t);