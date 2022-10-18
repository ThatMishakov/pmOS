#include "syscalls.hh"
#include "utils.hh"
#include "common/com.h"
#include "paging.hh"
#include "sched.hh"
#include "lib/vector.hh"

void syscall_handler(TaskDescriptor* task)
{
    Interrupt_Register_Frame* regs = &task->regs;
    ReturnStr<uint64_t> r = {};
    // TODO: check permissions

    uint64_t call_n = regs->rdi;

    switch (call_n) {
    case SYSCALL_GET_PAGE:
        r.result = get_page(regs->rsi);
        break;
    case SYSCALL_RELEASE_PAGE:
        r.result = release_page(regs->rsi);
        break;
    case SYSCALL_GETPID:
        r = getpid(task);
        break;
    case SYSCALL_CREATE_PROCESS:
        r = syscall_create_process();
        break;
    case SYSCALL_MAP_INTO:
        r.result = syscall_map_into();
        break;
    case SYSCALL_BLOCK:
        r = syscall_block(task);
        break;
    case SYSCALL_MAP_INTO_RANGE:
        r.result = syscall_map_into_range(task);
    default:
        // Not supported
        r.result = ERROR_UNSUPPORTED;
        break;
    }
    regs->rax = r.result;
    regs->rdx = r.val;
}

uint64_t get_page(uint64_t virtual_addr)
{
    // Check allignment to 4096K (page size)
    if (virtual_addr & 0xfff) return ERROR_UNALLIGNED;

    // Check that program is not hijacking kernel space
    if (virtual_addr >= KERNEL_ADDR_SPACE) return ERROR_OUT_OF_RANGE;

    // Check that the page is not already mapped
    if (page_type(virtual_addr) != Page_Types::UNALLOCATED) return ERROR_PAGE_PRESENT;

    // Everything seems ok, get the page (lazy allocation)
    Page_Table_Argumments arg = {};
    arg.user_access = 1;
    arg.writeable = 1;
    arg.execution_disabled = 0;
    uint64_t result = alloc_page_lazy(virtual_addr, arg);

    // Return the result (success or failure)
    return result;
}

uint64_t release_page(uint64_t virtual_addr)
{
    // Check allignment to 4096K (page size)
    if (virtual_addr & 0xfff) return ERROR_UNALLIGNED;

    // Check that program is not hijacking kernel space
    if (virtual_addr >= KERNEL_ADDR_SPACE) return ERROR_OUT_OF_RANGE;

    // Check page
    Page_Types p = page_type(virtual_addr);
    if (p == Page_Types::UNALLOCATED) return ERROR_PAGE_NOT_ALLOCATED;
    if (p != NORMAL) return ERROR_HUGE_PAGE;

    // Everything ok, release the page
    return release_page_s(virtual_addr);
}

ReturnStr<uint64_t> getpid(TaskDescriptor* d)
{
    return {SUCCESS, d->pid};
}

ReturnStr<uint64_t> syscall_create_process()
{
    return create_process();
}

kresult_t syscall_map_into()
{
    return ERROR_NOT_IMPLEMENTED;
}

ReturnStr<uint64_t> syscall_block(TaskDescriptor* current_task)
{
    kresult_t r = block_process(current_task);
    return {r, 0};
}

kresult_t syscall_map_into_range(TaskDescriptor* d)
{
    uint64_t pid = d->regs.rsi;
    uint64_t page_start = d->regs.rdx;
    uint64_t to_addr = d->regs.rcx;
    uint64_t nb_pages = d->regs.r8;
    Page_Table_Argumments pta = *(Page_Table_Argumments *)&d->regs.r9;

    // TODO: Check permissions

    // Check allignment
    if (page_start & 0xfff) return ERROR_UNALLIGNED;
    if (to_addr & 0xfff) return ERROR_UNALLIGNED;

    // Check if process exists
    if (not exists_process(pid)) return ERROR_NO_SUCH_PROCESS;

    // Check process status
    if (not is_uninited(pid)) return ERROR_PROCESS_INITED;

    // Get pid task_struct
    TaskDescriptor* t = get_task(pid);

    kresult_t r = transfer_pages(t, page_start, to_addr, nb_pages, pta);

    return r;
}