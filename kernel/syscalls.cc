#include "syscalls.hh"
#include "utils.hh"
#include <kernel/com.h>
#include "paging.hh"
#include "sched.hh"
#include "lib/vector.hh"
#include "asm.hh"
#include "messaging.hh"
#include <kernel/messaging.h>
#include <kernel/block.h>

extern "C" ReturnStr<uint64_t> syscall_handler(uint64_t call_n, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
    ReturnStr<uint64_t> r = {};
    // TODO: check permissions

    t_print("Debug: syscall %h pid %h\n", call_n, get_cpu_struct()->current_task->pid);
    switch (call_n) {
    case SYSCALL_GET_PAGE:
        r.result = get_page(arg1);
        break;
    case SYSCALL_RELEASE_PAGE:
        r.result = release_page(arg1);
        break;
    case SYSCALL_GETPID:
        r = getpid();
        break;
    case SYSCALL_CREATE_PROCESS:
        r = syscall_create_process();
        break;
    case SYSCALL_MAP_INTO:
        r.result = syscall_map_into();
        break;
    case SYSCALL_BLOCK:
        r = syscall_block(arg1);
        break;
    case SYSCALL_MAP_INTO_RANGE:
        r.result = syscall_map_into_range(arg1, arg2, arg3, arg4, arg5);
        break;
    case SYSCALL_GET_PAGE_MULTI:
        r.result = syscall_get_page_multi(arg1, arg2);
        break;
    case SYSCALL_START_PROCESS:
        r.result = syscall_start_process(arg1, arg2);
        break;
    case SYSCALL_EXIT:
        r.result = syscall_exit(arg1, arg2);
        break;
    case SYSCALL_MAP_PHYS:
        r.result = syscall_map_phys(arg1, arg2, arg3, arg4);
        break;
    case SYSCALL_GET_MSG_INFO:
        r.result = syscall_get_message_info(arg1);
        break;
    case SYSCALL_GET_MESSAGE:
        r.result = syscall_get_first_message(arg1, arg2);
        break;
    case SYSCALL_SEND_MSG_TASK:
        r.result = syscall_send_message_task(arg1, arg2, arg3, arg4);
        break;
    case SYSCALL_SEND_MSG_PORT:
        r.result = syscall_send_message_port(arg1, arg2, arg3);
        break;
    case SYSCALL_SET_PORT:
        r.result = syscall_set_port(arg1, arg2, arg3, arg4);
        break;
    default:
        // Not supported
        r.result = ERROR_NOT_SUPPORTED;
        break;
    }
    
    TaskDescriptor* t = get_cpu_struct()->current_task;
    if (t != nullptr) {
        t->regs.scratch_r.rax = r.result;
        t->regs.scratch_r.rdx = r.val;
    }

    return r;
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

ReturnStr<uint64_t> getpid()
{
    return {SUCCESS, get_cpu_struct()->current_task->pid};
}

ReturnStr<uint64_t> syscall_create_process()
{
    return create_process(3);
}

kresult_t syscall_map_into()
{
    return ERROR_NOT_IMPLEMENTED;
}

ReturnStr<uint64_t> syscall_block(uint64_t mask)
{
    ReturnStr<uint64_t> r = get_cpu_struct()->current_task->block(mask);
    return r;
}

kresult_t syscall_map_into_range(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
    uint64_t pid = arg1;
    uint64_t page_start = arg2;
    uint64_t to_addr = arg3;
    uint64_t nb_pages = arg4;

    Page_Table_Argumments pta = {};
    pta.user_access = 1;
    pta.global = 0;
    pta.writeable = arg5& 0x01;
    pta.execution_disabled = arg5&0x02;

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
            invalidade_noerr(virtual_addr + k*KB(4));

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
    t->set_entry_point(start);

    // Init task
    t->init_task();

    return SUCCESS;
}

kresult_t syscall_exit(uint64_t arg1, uint64_t arg2)
{
    TaskDescriptor* task = get_cpu_struct()->current_task;

    // Record exit code
    task->ret_hi = arg2;
    task->ret_lo = arg1;

    // Kill the process
    kill(task);

    return SUCCESS;
}

kresult_t syscall_map_phys(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
    uint64_t virt = arg1;
    uint64_t phys = arg2;
    uint64_t nb_pages = arg3;

    Page_Table_Argumments pta = {};
    pta.user_access = 1;
    pta.extra = 0b010;
    pta.global = 0;
    pta.writeable = arg4& 0x01;
    pta.execution_disabled = arg4&0x02;
    t_print("Debug: map_phys virt %h <- phys %h nb %h\n", virt, phys, nb_pages);

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
        for (uint64_t k = 0; k < i; ++i) {
            invalidade_noerr(virt+k*KB(4));
        }

    return r;
}

kresult_t syscall_get_first_message(uint64_t buff, uint64_t args)
{
    TaskDescriptor* current = get_cpu_struct()->current_task;

    current->lock.lock();

    kresult_t result = SUCCESS;

    if (current->messages.empty()) {
        current->lock.unlock();
        return ERROR_NO_MESSAGES;
    }

    Message& msg = current->messages.front();

    result = msg.copy_to_user_buff((char*)buff);

    if (result == SUCCESS) {
        if (!(args & MSG_ARG_NOPOP)) {
            current->messages.pop();
        }
    }

    current->lock.unlock();
    return result;
}

kresult_t syscall_send_message_task(uint64_t pid, uint64_t channel, uint64_t size, uint64_t message)
{
    // TODO: Check permissions

    uint64_t self_pid = get_cpu_struct()->current_task->pid;
    if (self_pid == pid) return ERROR_CANT_MESSAGE_SELF;

    if (not exists_process(pid)) return ERROR_NO_SUCH_PROCESS;
    TaskDescriptor* process = (*s_map).at(pid);
    kresult_t result = SUCCESS;

    process->lock.lock();
    result = queue_message(process, self_pid, channel, (char*)message, size);
    process->unblock_if_needed(MESSAGE_S_NUM);
    process->lock.unlock();

    return result;
}

kresult_t syscall_send_message_port(uint64_t port, size_t size, uint64_t message)
{
    // TODO: Check permissions

    TaskDescriptor* current = get_cpu_struct()->current_task;

    if (current->ports.count(port) == 0) return ERROR_PORT_NOT_EXISTS;

    Port_Desc* d = &current->ports.at(port);

    if (not exists_process(d->task)){
        current->ports.erase(port);
        return ERROR_PORT_CLOSED;
    }
    TaskDescriptor* process = (*s_map).at(d->task);
    kresult_t result = SUCCESS;

    process->lock.lock();
    result = queue_message(process, current->pid, d->channel, (char*)message, size);
    process->unblock_if_needed(MESSAGE_S_NUM);
    process->lock.unlock();

    return result;
}

kresult_t syscall_set_port(uint64_t pid, uint64_t port, uint64_t dest_pid, uint64_t dest_chan)
{
    // TODO: Check permissions

    if (pid != dest_pid) return ERROR_CANT_MESSAGE_SELF;

    if (not exists_process(pid)) return ERROR_NO_SUCH_PROCESS;
    if (not exists_process(dest_pid)) return ERROR_NO_SUCH_PROCESS;

    TaskDescriptor* process = (*s_map).at(pid);
    process->lock.lock();
    process->ports.insert({port, {dest_pid, dest_chan}});
    process->lock.unlock();

    return SUCCESS;
}

kresult_t syscall_get_message_info(uint64_t message_struct)
{
    TaskDescriptor* current = get_cpu_struct()->current_task;

    current->lock.lock();

    kresult_t result = SUCCESS;

    if (current->messages.empty()) {
        current->lock.unlock();
        return ERROR_NO_MESSAGES;
    }

    Message& msg = current->messages.front();
    uint64_t msg_struct_size = sizeof(Message_Descriptor);

    result = prepare_user_buff((char*)message_struct, msg_struct_size, true);

    if (result == SUCCESS) {
        Message_Descriptor& desc = *(Message_Descriptor*)message_struct;
        desc.sender = msg.from;
        desc.channel = msg.channel;
        desc.size = msg.size();
    }

    current->lock.unlock();
    return result;
}