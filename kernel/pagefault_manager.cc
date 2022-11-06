#include "pagefault_manager.hh"
#include "utils.hh"
#include "asm.hh"
#include "paging.hh"

void pagefault_manager(uint64_t err, Interrupt_Stackframe* int_s)
{
    // Get the memory location which has caused the fault
    uint64_t virtual_addr = getCR2();

    // Get the page
    virtual_addr &= ~0xfff;

    // Get page type
    Page_Types type = page_type(virtual_addr);

    t_print_bochs("Debug: Pagefault %h -> ", virtual_addr);

    switch (type) {
    case Page_Types::LAZY_ALLOC: // Lazilly allocated page caused the fault
        t_print_bochs("delayed allocation\n");
        get_lazy_page(virtual_addr);
        break;
    case Page_Types::UNALLOCATED: // Page not allocated
        t_print_bochs("unallocated... halting...\n");
        halt();
        break;
    default:
        // Not implemented
        t_print_bochs("%h not implemented (error %h). Halting...\n", type, err);
        halt();
        break;
    }
}