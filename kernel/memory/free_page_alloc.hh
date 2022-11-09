#pragma once
#include <types.hh>

#define FREE_PAGES_SIZE 16

kresult_t free_page_alloc_init();

ReturnStr<u64> get_free_page();

void release_free_page(u64);