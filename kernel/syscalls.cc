#include "syscalls.hh"
#include "utils.hh"

void syscall_handler(Interrupt_Register_Frame* regs)
{
    // TODO: check permissions

    uint64_t call_n = regs->rdi;

    switch (call_n) {
    case SYSCALL_GET_PAGE:
        regs->rax = get_page(regs);
        break;

    default:
        // Not supported
        regs->rax = ERROR_UNSUPPORTED;
        break;
    }
}

uint64_t get_page(Interrupt_Register_Frame* regs)
{
    t_print("Reached get_page() syscall. Returning ERROR_NOT_IMPLEMENTED %h\n", ERROR_NOT_IMPLEMENTED);
    return ERROR_NOT_IMPLEMENTED;
}