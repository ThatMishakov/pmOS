#include "common/com.h"
#include "gdt.hh"
#include "utils.hh"
#include "misc.hh"
#include "mem.hh"
#include "interrupts.hh"
#include "malloc.hh"
#include "sched.hh"
#include "messaging.hh"
#include "free_page_alloc.hh"
#include "common/errors.h"

Kernel_Entry_Data* kdata;

extern "C" int _start(Kernel_Entry_Data* d)
{
    kdata = d;
    t_print("Hello from kernel!\n");
    t_print("Initializing GDT...\n");
    init_gdt();

    t_print("Initializing page frame allocator\n");
    palloc.init(d->mem_bitmap, d->mem_bitmap_size);
    palloc.init_after_paging();

    t_print("Allocating and freeing a page just to test...\n");
<<<<<<< HEAD
    void *page = palloc.alloc_page();
    palloc.free(page);
=======
    ReturnStr<void*> page = palloc.alloc_page();
    if (page.result != SUCCESS) {
        t_print("Error %h! Returning...\n", page.result); 
        return page.result;
    }
    palloc.free(page.val);
>>>>>>> refs/remotes/origin/main

    free_page_alloc_init();

    t_print("Initializing scheduling...\n");
    init_scheduling();

    t_print("Initializing interrupts...\n");
    init_interrupts();

    t_print("Invoking interrupt 0xCA...\n");
    asm("int $0xCA");

    t_print("Returning to loader\n");
    return 0xdeadc0de;
}
