#include <processes/syscalls.hh>
#include <utils.hh>
#include <kernel/com.h>
#include <memory/paging.hh>
#include <processes/sched.hh>
#include <lib/vector.hh>
#include <asm.hh>
#include <messaging/messaging.hh>
#include <kernel/messaging.h>
#include <kernel/block.h>
#include <kernel/attributes.h>
#include <kernel/flags.h>

extern "C" ReturnStr<u64> syscall_handler(u64 call_n, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    ReturnStr<u64> r = {};
    // TODO: check permissions

    TaskDescriptor& task = *get_cpu_struct()->current_task;
    //t_print_bochs("Debug: syscall %h pid %h\n", call_n, get_cpu_struct()->current_task->pid);
    if (task.attr.debug_syscalls) {
        t_print("Debug: syscall %h pid %h\n", call_n, get_cpu_struct()->current_task->pid);
    }
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
    case SYSCALL_RELEASE_PAGE_MULTI:
        r.result = syscall_release_page_multi(arg1, arg2);
        break;
    case SYSCALL_START_PROCESS:
        r.result = syscall_start_process(arg1, arg2, arg3, arg4, arg5);
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
    case SYSCALL_SET_PORT_KERNEL:
        r.result = syscall_set_port_kernel(arg1, arg2, arg3);
        break;
    case SYSCALL_SET_ATTR:
        r.result = syscall_set_attribute(arg1, arg2, arg3);
        break;
    case SYSCALL_INIT_STACK:
        r = syscall_init_stack(arg1, arg2);
        break;
    case SYSCALL_SHARE_WITH_RANGE:
        r.result = syscall_share_with_range(arg1, arg2, arg3, arg4, arg5);
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

u64 get_page(u64 virtual_addr)
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
    u64 result = alloc_page_lazy(virtual_addr, arg);

    // Return the result (success or failure)
    return result;
}

u64 release_page(u64 virtual_addr)
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
    return release_page_s(virtual_addr, get_cpu_struct()->current_task->pid);
}

ReturnStr<u64> getpid()
{
    return {SUCCESS, get_cpu_struct()->current_task->pid};
}

ReturnStr<u64> syscall_create_process()
{
    return create_process(3);
}

kresult_t syscall_map_into()
{
    return ERROR_NOT_IMPLEMENTED;
}

ReturnStr<u64> syscall_block(u64 mask)
{
    ReturnStr<u64> r = get_cpu_struct()->current_task->block(mask);
    return r;
}

kresult_t syscall_map_into_range(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    u64 pid = arg1;
    u64 page_start = arg2;
    u64 to_addr = arg3;
    u64 nb_pages = arg4;

    Page_Table_Argumments pta = {};
    pta.user_access = 1;
    pta.global = 0;
    pta.writeable = !!(arg5&FLAG_RW);
    pta.execution_disabled = !!(arg5&FLAG_NOEXECUTE);

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

    TaskDescriptor* current = get_cpu_struct()->current_task;
    kresult_t r = transfer_pages(current, t, page_start, to_addr, nb_pages, pta);

    return r;
}

kresult_t syscall_share_with_range(u64 pid, u64 page_start, u64 to_addr, u64 nb_pages, u64 args)
{
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

    Page_Table_Argumments pta = {};
    pta.user_access = 1;
    pta.global = 0;
    pta.writeable = args& FLAG_RW;
    pta.execution_disabled = args&FLAG_NOEXECUTE;

    kresult_t r = share_pages(t, page_start, to_addr, nb_pages, pta);

    return r;
}

kresult_t syscall_get_page_multi(u64 virtual_addr, u64 nb_pages)
{
    // Check allignment to 4096K (page size)
    if (virtual_addr & 0xfff) return ERROR_UNALLIGNED;

    // Check that program is not hijacking kernel space
    if (virtual_addr >= KERNEL_ADDR_SPACE or (virtual_addr + nb_pages*KB(4)) > KERNEL_ADDR_SPACE or (virtual_addr + nb_pages*KB(4) < virtual_addr)) return ERROR_OUT_OF_RANGE;

    // Everything seems ok, get the page (lazy allocation)
    Page_Table_Argumments arg = {};
    arg.user_access = 1;
    arg.writeable = 1;
    arg.execution_disabled = 1;
    u64 result = SUCCESS;
    u64 i = 0;
    for (; i < nb_pages and result == SUCCESS; ++i)
        result = alloc_page_lazy(virtual_addr + i*KB(4), arg);

    // If unsuccessfull, return everything back
    if (result != SUCCESS)
        for (u64 k = 0; k < i; ++k) 
            invalidade_noerr(virtual_addr + k*KB(4));

    // Return the result (success or failure)
    return result;
}

kresult_t syscall_release_page_multi(u64 virtual_addr, u64 nb_pages)
{
    // Check allignment to 4096K (page size)
    if (virtual_addr & 0xfff) return ERROR_UNALLIGNED;

    // Check that program is not hijacking kernel space
    if (virtual_addr >= KERNEL_ADDR_SPACE or (virtual_addr + nb_pages*KB(4)) > KERNEL_ADDR_SPACE or (virtual_addr + nb_pages*KB(4) < virtual_addr)) return ERROR_OUT_OF_RANGE;

    // Check pages
    for (u64 i = 0; i < nb_pages; ++i) {
        Page_Types p = page_type(virtual_addr);
        if (p == Page_Types::UNALLOCATED) return ERROR_PAGE_NOT_ALLOCATED;
        if (p != NORMAL) return ERROR_HUGE_PAGE;
    }

    kresult_t r = SUCCESS;

    // TODO: Leaves user process in unknown state afterwards

    // Everything ok, release pages
    for (u64 i = 0; i < nb_pages and r == SUCCESS; ++i)
        r = release_page_s(virtual_addr + i*KB(4), get_cpu_struct()->current_task->pid);

    // Return the result (success or failure)
    return r;
}

kresult_t syscall_start_process(u64 pid, u64 start, u64 arg1, u64 arg2, u64 arg3)
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

    // Pass arguments
    t->regs.scratch_r.rdi = arg1;
    t->regs.scratch_r.rsi = arg2;
    t->regs.scratch_r.rdx = arg3;

    // Init task
    t->init_task();

    return SUCCESS;
}

ReturnStr<u64> syscall_init_stack(u64 pid, u64 esp)
{
    ReturnStr<u64> r = {ERROR_GENERAL, 0};
    // TODO: Check permissions

    // Check if process exists
    if (not exists_process(pid)) {
        r.result = ERROR_NO_SUCH_PROCESS;
        return r;
    }

    // Check process status
    if (not is_uninited(pid)) {
        r.result = ERROR_PROCESS_INITED;
        return r;
    }

    // Get task descriptor
    TaskDescriptor* t = get_task(pid);

    if (esp == 0) { // If ESP == 0 use default kernel's stack policy
        r = t->init_stack();
    } else {
        t->regs.e.rsp = esp;
        r = {SUCCESS, esp};
    }

    return r;
}

kresult_t syscall_exit(u64 arg1, u64 arg2)
{
    TaskDescriptor* task = get_cpu_struct()->current_task;

    // Record exit code
    task->ret_hi = arg2;
    task->ret_lo = arg1;

    // Kill the process
    kill(task);

    return SUCCESS;
}

kresult_t syscall_map_phys(u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
    u64 virt = arg1;
    u64 phys = arg2;
    u64 nb_pages = arg3;

    Page_Table_Argumments pta = {};
    pta.user_access = 1;
    pta.extra = 0b010;
    pta.global = 0;
    pta.writeable = arg4& 0x01;
    pta.execution_disabled = arg4&0x02;
    //t_print("Debug: map_phys virt %h <- phys %h nb %h\n", virt, phys, nb_pages);

    // TODO: Check permissions

    // Check if legal address
    if (virt >= KERNEL_ADDR_SPACE or (virt + nb_pages*KB(4)) > KERNEL_ADDR_SPACE or (virt + nb_pages*KB(4) < virt)) return ERROR_OUT_OF_RANGE;

    // Check allignment
    if (virt & 0xfff) return ERROR_UNALLIGNED;
    if (phys & 0xfff) return ERROR_UNALLIGNED;

    // TODO: Check if physical address is ok

    u64 i = 0;
    kresult_t r = SUCCESS;
    for (; i < nb_pages and r == SUCCESS; ++i) {
        r = map(phys + i*KB(4), virt+i*KB(4), pta);
    }

    --i;

    // If was not successfull, invalidade the pages
    if (r != SUCCESS)
        for (u64 k = 0; k < i; ++k) {
            invalidade_noerr(virt+k*KB(4));
        }

    return r;
}

kresult_t syscall_get_first_message(u64 buff, u64 args)
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

kresult_t syscall_send_message_task(u64 pid, u64 channel, u64 size, u64 message)
{
    // TODO: Check permissions

    u64 self_pid = get_cpu_struct()->current_task->pid;
    if (self_pid == pid) return ERROR_CANT_MESSAGE_SELF;

    if (not exists_process(pid)) return ERROR_NO_SUCH_PROCESS;
    TaskDescriptor* process = s_map.at(pid);
    kresult_t result = SUCCESS;

    process->lock.lock();
    result = queue_message(process, self_pid, channel, (char*)message, size);
    process->unblock_if_needed(MESSAGE_S_NUM);
    process->lock.unlock();

    return result;
}

kresult_t syscall_send_message_port(u64 port, size_t size, u64 message)
{
    // TODO: Check permissions

    TaskDescriptor* current = get_cpu_struct()->current_task;

    kresult_t result = current->ports.send_from_user(current->pid, port, message, size);

    return result;
}

kresult_t syscall_set_port(u64 pid, u64 port, u64 dest_pid, u64 dest_chan)
{
    // TODO: Check permissions

    if (pid == dest_pid) return ERROR_CANT_MESSAGE_SELF;

    if (not exists_process(pid)) return ERROR_NO_SUCH_PROCESS;

    TaskDescriptor* process = s_map.at(pid);

    process->lock.lock();
    kresult_t result = process->ports.set_port(port, dest_pid, dest_chan);
    process->lock.unlock();

    return result;
}

kresult_t syscall_set_port_kernel(u64 port, u64 dest_pid, u64 dest_chan)
{
    // TODO: Check permissions

    messaging_ports.lock();
    kresult_t result = kernel_ports.set_port(port, dest_pid, dest_chan);
    messaging_ports.unlock();

    return result;
}

kresult_t syscall_get_message_info(u64 message_struct)
{
    TaskDescriptor* current = get_cpu_struct()->current_task;

    current->lock.lock();

    kresult_t result = SUCCESS;

    if (current->messages.empty()) {
        current->lock.unlock();
        return ERROR_NO_MESSAGES;
    }

    Message& msg = current->messages.front();
    u64 msg_struct_size = sizeof(Message_Descriptor);

    result = prepare_user_buff_wr((char*)message_struct, msg_struct_size);

    if (result == SUCCESS) {
        Message_Descriptor& desc = *(Message_Descriptor*)message_struct;
        desc.sender = msg.from;
        desc.channel = msg.channel;
        desc.size = msg.size();
    }

    current->lock.unlock();
    return result;
}

kresult_t syscall_set_attribute(u64 pid, u64 attribute, u64 value)
{
    // TODO: Check persmissions

    if (not exists_process(pid)) return ERROR_NO_SUCH_PROCESS;
    TaskDescriptor* process = s_map.at(pid);

    kresult_t result = ERROR_GENERAL;

    switch (attribute) {
    case ATTR_ALLOW_PORT:
        process->regs.e.rflags.bits.iopl = value ? 3 : 0;
        result = SUCCESS;
        break;
    case ATTR_DEBUG_SYSCALLS:
        process->attr.debug_syscalls = value;
        result = SUCCESS;
        break;
    default:
        result = ERROR_NOT_SUPPORTED;
        break;
    }

    return result;
}