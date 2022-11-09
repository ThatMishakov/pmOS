#include "exceptions_managers.hh"
#include <utils.hh>
#include <asm.hh>
#include <memory/paging.hh>
#include <processes/syscalls.hh>

void pagefault_manager(u64 err, Interrupt_Stackframe* int_s)
{
    // Get the memory location which has caused the fault
    u64 virtual_addr = getCR2();

    // Get the page
    u64 page = virtual_addr&~0xfff;

    // Get page type
    Page_Types type = page_type(page);

    t_print("Debug: Pagefault %h pid %i rip %h error %h -> ", virtual_addr, get_cpu_struct()->current_task->pid, int_s->rip, err);

    switch (type) {
    case Page_Types::LAZY_ALLOC: // Lazilly allocated page caused the fault
        t_print("delayed allocation\n");
        get_lazy_page(page);
        break;
    case Page_Types::UNALLOCATED: // Page not allocated
        t_print("unallocated... killing process...\n");
        syscall_exit(4, 0);
        break;
    default:
        // Not implemented
        t_print("%h not implemented (error %h). Halting...\n", type, err);
        halt();
        break;
    }
}