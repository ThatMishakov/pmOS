#include "free_page_alloc.hh"
#include "lib/list.hh"
#include "misc.hh"
#include "types.hh"

List<uint64_t>* free_pages_list = nullptr;

DECLARE_LOCK(free_page_alloc);

void add_pages(uint64_t pages)
{
    uint64_t start = (uint64_t)unoccupied;
    uint64_t size = FREE_PAGES_SIZE*4096;

    unoccupied = (void*)((uint64_t)unoccupied + size);

    for (int i = 0; i < size; i += 1024)
        free_pages_list->push_back(i+start);
}

void free_page_alloc_init()
{
    free_pages_list = new List<uint64_t>;

    add_pages(FREE_PAGES_SIZE);
}