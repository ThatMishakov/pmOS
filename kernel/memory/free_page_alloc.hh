#pragma once
#include <types.hh>
#include <lib/array.hh>
#include <lib/list.hh>

#define FREE_PAGES_SIZE 16

class Free_Page_Alloc {
private:
    klib::list<u64> free_pages_list;
    DECLARE_LOCK(free_page_alloc);
    void add_pages(u64 pages);
public:
    ReturnStr<u64> get_free_page();
    void release_free_page(u64);
};

extern Free_Page_Alloc global_free_page;

template<size_t size>
class Free_Page_Alloc_Static {
    klib::array<u64, size> pages;
    size_t index;

public:
    Free_Page_Alloc_Static();
    ~Free_Page_Alloc_Static();
    u64 get_free_page();
    void release_free_page(u64);
};

template<size_t size>
Free_Page_Alloc_Static<size>::Free_Page_Alloc_Static()
{
    index = size - 1;
    for (size_t i = 0 ; i < size; ++i) {
        pages[i] = global_free_page.get_free_page().val;
    }
}

template<size_t size>
Free_Page_Alloc_Static<size>::~Free_Page_Alloc_Static()
{
    for (size_t i = index ; i < size; ++i) {
        global_free_page.release_free_page(pages[i]);
    }
}

template<size_t size>
void Free_Page_Alloc_Static<size>::release_free_page(u64 page)
{
    pages[index++] = page;
}

template<size_t size>
u64 Free_Page_Alloc_Static<size>::get_free_page()
{
    return pages[index--];
}