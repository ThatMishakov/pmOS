#pragma once
#include <stdint.h>

#define FREE_PAGES_SIZE 128

void free_page_alloc_init();

uint64_t get_free_page();

void release_free_page();