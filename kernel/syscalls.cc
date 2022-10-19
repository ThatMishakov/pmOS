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
        t_print("Debug: Syscall getpage\n");
        r.result = get_page(regs->rsi);
        break;
    case SYSCALL_RELEASE_PAGE:
        t_print("Debug: Syscall release_page\n");
        r.result = release_page(regs->rsi);
        break;
    case SYSCALL_GETPID:
        t_print("Debug: Syscall getpid\n");
        r = getpid(task);
        break;
    case SYSCALL_CREATE_PROCESS:
        t_print("Debug: Syscall create_process\n");
        r = syscall_create_process(regs->rsi);
        break;
    case SYSCALL_MAP_INTO:
        t_print("Debug: Syscall map_into\n");
        r.result = syscall_map_into();
        break;
    case SYSCALL_BLOCK:
        t_print("Debug: Syscall block\n");
        r = syscall_block(task);
        break;
    case SYSCALL_MAP_INTO_RANGE:
        t_print("Debug: Syscall map_into_range\n");
        r.result = syscall_map_into_range(task);
        break;
    case SYSCALL_GET_PAGE_MULTI:
        t_print("Debug: Syscall get_pages_multi %h %h\n", regs->rsi, regs->rdx);
        r.result = syscall_get_page_multi(regs->rsi, regs->rdx);
        break;
    case SYSCALL_START_PROCESS:
        t_print("Debug: Syscall start_process\n");
        r.result = syscall_start_process(regs->rsi, regs->rdx);
        break;
    default:
        // Not supported
        r.result = ERROR_NOT_SUPPORTED;
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

ReturnStr<uint64_t> syscall_create_process(uint8_t ring)
{
    return create_process(ring);
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

    // Check if legal address
    if (page_start >= KERNEL_ADDR_SPACE or (page_start + nb_pages*KB(4)) > KERNEL_ADDR_SPACE) return ERROR_OUT_OF_RANGE;

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

kresult_t syscall_get_page_multi(uint64_t virtual_addr, uint64_t nb_pages)
{
    // Check allignment to 4096K (page size)
    if (virtual_addr & 0xfff) return ERROR_UNALLIGNED;

    // Check that program is not hijacking kernel space
    if (virtual_addr >= KERNEL_ADDR_SPACE or (virtual_addr + nb_pages*KB(4)) > KERNEL_ADDR_SPACE) return ERROR_OUT_OF_RANGE;

    // Everything seems ok, get the page (lazy allocation)
    Page_Table_Argumments arg = {};
    arg.user_access = 1;
    arg.writeable = 1;
    arg.execution_disabled = 0;
    uint64_t result = SUCCESS;
    uint64_t i = 0;
    for (; i < nb_pages and result == SUCCESS; ++i)
        result = alloc_page_lazy(virtual_addr + i*KB(4), arg);

    // If unsuccessfull, return everything back
    if (result != SUCCESS)
        for (uint64_t k = 0; k < i; ++k) 
            get_pte(virtual_addr + k*KB(4)) = {};

    // Return the result (success or failure)
    return result;
}

kresult_t syscall_start_process(uint64_t pid, uint64_t start)
{
    // TODO: Check permissions

    // Check if process exists
    if (not exists_process(pid)) return ERROR_NO_SUCH_PROCESS;

    // Check process status
    if (not is_uninited(pid)) return ERROR_PROCESS_INITED;

    // Get task descriptor
    TaskDescriptor* t = get_task(pid);

    // Set entry
    set_entry_point(t, start);

    // Init task
    init_task(t);

    return SUCCESS;
}