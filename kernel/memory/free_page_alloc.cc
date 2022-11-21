#include "free_page_alloc.hh"
#include <lib/list.hh>
#include <misc.hh>
#include "types.hh"
#include <kernel/errors.h>
#include <utils.hh>
#include "malloc.hh"

klib::list<u64> free_pages_list;
Free_Page_Alloc global_free_page;

DECLARE_LOCK(free_page_alloc);

void Free_Page_Alloc::add_pages(u64 pages)
{
    u64 start = (u64)unoccupied;
    u64 size = pages*4096;

    unoccupied = (void*)((u64)unoccupied + size);

    for (u64 i = 0; i < size; i += 4096)
        free_pages_list.push_back(i+start);
}

void Free_Page_Alloc::release_free_page(u64 page)
{
    lock.lock();
    free_pages_list.push_back(page);
    lock.unlock();
}

ReturnStr<u64> Free_Page_Alloc::get_free_page()
{
    lock.lock();
    // TODO: Error checking
    if (free_pages_list.empty()) add_pages(FREE_PAGES_SIZE);

    u64 page = free_pages_list.front();
    free_pages_list.pop_front();
    lock.unlock();
    return {SUCCESS, page};
}