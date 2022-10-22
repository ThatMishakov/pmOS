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
        r = syscall_create_process(regs->rsi);
        break;
    case SYSCALL_MAP_INTO:
        r.result = syscall_map_into();
        break;
    case SYSCALL_BLOCK:
        r = syscall_block(task);
        break;
    case SYSCALL_MAP_INTO_RANGE:
        r.result = syscall_map_into_range(task);
        break;
    case SYSCALL_GET_PAGE_MULTI:
        r.result = syscall_get_page_multi(regs->rsi, regs->rdx);
        break;
    case SYSCALL_START_PROCESS:
        r.result = syscall_start_process(regs->rsi, regs->rdx);
        break;
    case SYSCALL_EXIT:
        r.result = syscall_exit(task);
        break;
    case SYSCALL_MAP_PHYS:
        r.result = syscall_map_phys(task);
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
    if (page_start >= KERNEL_ADDR_SPACE or (page_start + nb_pages*KB(4)) > KERNEL_ADDR_SPACE or (page_start + nb_pages*KB(4) < page_start)) return ERROR_OUT_OF_RANGE;

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
    if (virtual_addr >= KERNEL_ADDR_SPACE or (virtual_addr + nb_pages*KB(4)) > KERNEL_ADDR_SPACE or (virtual_addr + nb_pages*KB(4) < virtual_addr)) return ERROR_OUT_OF_RANGE;

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
            *get_pte(virtual_addr + k*KB(4)) = {};

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

kresult_t syscall_exit(TaskDescriptor* task)
{
    // Record exit code
    task->ret_hi = task->regs.rdx;
    task->ret_lo = task->regs.rax;

    // Kill the process
    kill(task);

    return SUCCESS;
}

kresult_t syscall_map_phys(TaskDescriptor* t)
{
    uint64_t virt = t->regs.rsi;
    uint64_t phys = t->regs.rdx;
    uint64_t nb_pages = t->regs.rcx;
    Page_Table_Argumments pta;
    pta.global = 0;
    pta.writeable = 1;
    pta.user_access = 1;
    pta.execution_disabled = 1;
    pta.extra = 0b010;

    // TODO: Check permissions

    // Check if legal address
    if (virt >= KERNEL_ADDR_SPACE or (virt + nb_pages*KB(4)) > KERNEL_ADDR_SPACE or (virt + nb_pages*KB(4) < virt)) return ERROR_OUT_OF_RANGE;

    // Check allignment
    if (virt & 0xfff) return ERROR_UNALLIGNED;
    if (phys & 0xfff) return ERROR_UNALLIGNED;

    // TODO: Check if physical address is ok

    uint64_t i = 0;
    kresult_t r = SUCCESS;
    for (; i < nb_pages and r == SUCCESS; ++i) {
        r = map(phys + i*KB(4), virt+i*KB(4), pta);
    }

    // If was not successfull, invalidade the pages
    if (r != SUCCESS)
        for (uint64_t k; k < i; ++i) {
            invalidade(virt+k*KB(4));
        }

    return r;
}