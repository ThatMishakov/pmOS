#include "free_page_alloc.hh"
#include "lib/list.hh"
#include "misc.hh"
#include "types.hh"
#include <kernel/errors.h>
#include "utils.hh"
#include "../malloc.hh"

klib::list<u64>* free_pages_list = nullptr;

DECLARE_LOCK(free_page_alloc);

void add_pages(u64 pages)
{
    u64 start = (u64)unoccupied;
    u64 size = pages*4096;

    unoccupied = (void*)((u64)unoccupied + size);

    for (u64 i = 0; i < size; i += 4096)
        free_pages_list->push_back(i+start);
}

kresult_t free_page_alloc_init()
{
    // TODO: Error checking
    free_pages_list = new klib::list<u64>;

    add_pages(FREE_PAGES_SIZE);
    return SUCCESS;
}

void release_free_page(u64 page)
{
    free_pages_list->push_back(page);
}

ReturnStr<u64> get_free_page()
{
    // TODO: Error checking
    if (free_pages_list->empty()) add_pages(FREE_PAGES_SIZE);

    u64 page = free_pages_list->front();
    free_pages_list->pop_front();
    return {SUCCESS, page};
}