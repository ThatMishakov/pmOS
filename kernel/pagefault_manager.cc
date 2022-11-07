#include "pagefault_manager.hh"
#include "utils.hh"
#include "asm.hh"
#include "paging.hh"

void pagefault_manager(uint64_t err, Interrupt_Stackframe* int_s)
{
    // Get the memory location which has caused the fault
    uint64_t virtual_addr = getCR2();

    // Get the page
    uint64_t page = virtual_addr&~0xfff;

    // Get page type
    Page_Types type = page_type(page);

    t_print("Debug: Pagefault %h pid %i rip %h error %h -> ", virtual_addr, get_cpu_struct()->current_task->pid, int_s->rip, err);

    switch (type) {
    case Page_Types::LAZY_ALLOC: // Lazilly allocated page caused the fault
        t_print("delayed allocation\n");
        get_lazy_page(page);
        break;
    case Page_Types::UNALLOCATED: // Page not allocated
        t_print("unallocated... halting...\n");
        halt();
        break;
    default:
        // Not implemented
        t_print("%h not implemented (error %h). Halting...\n", type, err);
        halt();
        break;
    }
}