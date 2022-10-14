#include "syscalls.hh"
#include "utils.hh"
#include "common/com.h"
#include "paging.hh"

void syscall_handler(Interrupt_Register_Frame* regs)
{
    // TODO: check permissions

    uint64_t call_n = regs->rdi;

    switch (call_n) {
    case SYSCALL_GET_PAGE:
        regs->rax = get_page(regs);
        break;

    case SYSCALL_RELEASE_PAGE:
        regs->rax = release_page(regs);
        break;

    default:
        // Not supported
        regs->rax = ERROR_UNSUPPORTED;
        break;
    }
}

uint64_t get_page(Interrupt_Register_Frame* regs)
{
    // %RSI -> second argument of syscall, page to be obtained
    uint64_t virtual_addr = regs->rsi;

    // Check allignment to 4096K (page size)
    if (virtual_addr & 0xfff) return ERROR_UNALLIGNED;

    // Check that program is not hijacking kernel space
    if (virtual_addr >= KERNEL_ADDR_SPACE) return ERROR_OUT_OF_RANGE;

    // Check that the page is not already mapped
    if (is_allocated(virtual_addr)) return ERROR_PAGE_PRESENT;

    // Everything seems ok, get the page
    Page_Table_Argumments arg = {};
    arg.user_access = 1;
    arg.writeable = 1;
    arg.execution_disabled = 0;
    uint64_t result = get_page_zeroed(virtual_addr, arg);

    // Return the result (success or failure)
    return result;
}

uint64_t release_page(Interrupt_Register_Frame* regs)
{
    // Not yet implemented
    return ERROR_NOT_IMPLEMENTED;
}