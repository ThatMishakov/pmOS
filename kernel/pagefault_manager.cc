#include "pagefault_manager.hh"
#include "utils.hh"
#include "asm.hh"
#include "paging.hh"

void pagefault_manager(void)
{
    // Get the memory location which has caused the fault
    uint64_t virtual_addr = getCR2();

    // Get the page
    virtual_addr &= ~0xfff;

    // Get page type
    Page_Types type = page_type(virtual_addr);

    t_print("Gotten pagefault page %h type %i\n", virtual_addr, type);

    switch (type) {
    case Page_Types::LAZY_ALLOC: // Lazilly allocated page caused the fault
        get_lazy_page(virtual_addr);
        break;
    case Page_Types::UNALLOCATED: // Page not allocated
        t_print("--> unallocated... halting...\n");
        halt();
        break;
    default:
        // Not implemented
        t_print("--> not implemented. Halting...\n");
        halt();
        break;
    }
}