#include "free_page_alloc.hh"
#include "lib/list.hh"
#include "misc.hh"
#include "types.hh"
#include "common/errors.h"
#include "utils.hh"

List<uint64_t>* free_pages_list = nullptr;

DECLARE_LOCK(free_page_alloc);

void add_pages(uint64_t pages)
{
    uint64_t start = (uint64_t)unoccupied;
    uint64_t size = pages*4096;

    unoccupied = (void*)((uint64_t)unoccupied + size);

    for (uint64_t i = 0; i < size; i += 1024)
        free_pages_list->push_back(i+start);
}

kresult_t free_page_alloc_init()
{
    // TODO: Error checking
    free_pages_list = new List<uint64_t>;

    add_pages(FREE_PAGES_SIZE);
    return SUCCESS;
}

void release_free_page(uint64_t page)
{
    free_pages_list->push_back(page);
}

ReturnStr<uint64_t> get_free_page()
{
    // TODO: Error checking
    if (free_pages_list->empty()) add_pages(FREE_PAGES_SIZE);

    uint64_t page = free_pages_list->front();
    free_pages_list->pop_front();
    return {SUCCESS, page};
}