#include "common/com.h"
#include "gdt.hh"
#include "utils.hh"
#include "misc.hh"
#include "mem.hh"

extern "C" int _start(Kernel_Entry_Data* d)
{
    t_print("Iniaializing kernel\n");
    t_print("Initializing GDT...\n");
    init_gdt();

    t_print("Initializing page frame allocator\n");
    palloc.init(d->mem_bitmap, d->mem_bitmap_size);
    palloc.init_after_paging();

    t_print("Allocating and freeing a page just to test...\n");
    void * page = palloc.alloc_page();
    palloc.free(page);

    t_print("Returning to loader\n");
    return 0xdeadc0de;
}