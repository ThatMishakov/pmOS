#include <processes/syscalls.hh>
#include <utils.hh>
#include <kernel/com.h>
#include <memory/paging.hh>
#include <sched/sched.hh>
#include <lib/vector.hh>
#include <asm.hh>
#include <messaging/messaging.hh>
#include <kernel/messaging.h>
#include <kernel/block.h>
#include <kernel/attributes.h>
#include <kernel/flags.h>
#include <interrupts/apic.hh>
#include <cpus/cpus.hh>
#include <interrupts/pit.hh>
#include <sched/sched.hh>

extern "C" void syscall_handler()
{
    klib::shared_ptr<TaskDescriptor> task = get_cpu_struct()->current_task;
    u64 call_n = task->regs.scratch_r.rdi;
    u64 arg1 = syscall_arg1(task);
    u64 arg2 = syscall_arg2(task);
    u64 arg3 = syscall_arg3(task);
    u64 arg4 = syscall_arg4(task);
    u64 arg5 = syscall_arg5(task);

    // TODO: check permissions

    //t_print_bochs("Debug: syscall %h pid %h", call_n, get_cpu_struct()->current_task->pid);
    if (task->attr.debug_syscalls) {
        t_print("Debug: syscall %h pid %h\n", call_n, get_cpu_struct()->current_task->pid);
    }

    switch (call_n) {
    case SYSCALL_GET_PAGE:
        get_page(arg1);
        break;
    case SYSCALL_RELEASE_PAGE:
        release_page(arg1);
        break;
    case SYSCALL_GETPID:
        getpid();
        break;
    case SYSCALL_CREATE_PROCESS:
        syscall_create_process();
        break;
    case SYSCALL_MAP_INTO:
        syscall_map_into();
        break;
    case SYSCALL_BLOCK:
        syscall_block(arg1);
        break;
    case SYSCALL_MAP_INTO_RANGE:
        syscall_map_into_range(arg1, arg2, arg3, arg4, arg5);
        break;
    case SYSCALL_GET_PAGE_MULTI:
        syscall_get_page_multi(arg1, arg2);
        break;
    case SYSCALL_RELEASE_PAGE_MULTI:
        syscall_release_page_multi(arg1, arg2);
        break;
    case SYSCALL_START_PROCESS:
        syscall_start_process(arg1, arg2, arg3, arg4, arg5);
        break;
    case SYSCALL_EXIT:
        syscall_exit(arg1, arg2);
        break;
    case SYSCALL_MAP_PHYS:
        syscall_map_phys(arg1, arg2, arg3, arg4);
        break;
    case SYSCALL_GET_MSG_INFO:
        syscall_get_message_info(arg1);
        break;
    case SYSCALL_GET_MESSAGE:
        syscall_get_first_message(arg1, arg2);
        break;
    case SYSCALL_SEND_MSG_TASK:
        syscall_send_message_task(arg1, arg2, arg3, arg4);
        break;
    case SYSCALL_SEND_MSG_PORT:
        syscall_send_message_port(arg1, arg2, arg3);
        break;
    case SYSCALL_SET_PORT:
        syscall_set_port(arg1, arg2, arg3, arg4);
        break;
    case SYSCALL_SET_PORT_KERNEL:
        syscall_set_port_kernel(arg1, arg2, arg3);
        break;
    case SYSCALL_SET_PORT_DEFAULT:
        syscall_set_port_default(arg1, arg2, arg3);
        break;
    case SYSCALL_SET_ATTR:
        syscall_set_attribute(arg1, arg2, arg3);
        break;
    case SYSCALL_INIT_STACK:
        syscall_init_stack(arg1, arg2);
        break;
    case SYSCALL_SHARE_WITH_RANGE:
        syscall_share_with_range(arg1, arg2, arg3, arg4, arg5);
        break;
    case SYSCALL_IS_PAGE_ALLOCATED:
        syscall_is_page_allocated(arg1);
        break;
    case SYSCALL_CONFIGURE_SYSTEM:
        syscall_configure_system(arg1, arg2, arg3);
        break;
    case SYSCALL_SET_PRIORITY:
        syscall_set_priority(arg1);
        break;
    case SYSCALL_GET_LAPIC_ID:
        syscall_get_lapic_id();
        break;
    default:
        // Not supported
        syscall_ret_low(task) = ERROR_NOT_SUPPORTED;
        break;
    }
    //t_print_bochs(" -> result %h\n", syscall_ret_low(task));
}

void get_page(u64 virtual_addr)
{
    const task_ptr& task = get_current_task();

    // Check allignment to 4096K (page size)
    if (virtual_addr & 0xfff) {
        syscall_ret_low(task) = ERROR_UNALLIGNED;
        return;
    }


    // Check that program is not hijacking kernel space
    if (virtual_addr >= KERNEL_ADDR_SPACE) {
        syscall_ret_low(task) = ERROR_OUT_OF_RANGE;
        return;
    }

    // Check that the page is not already mapped
    if (page_type(virtual_addr) != Page_Types::UNALLOCATED) {
        syscall_ret_low(task) = ERROR_PAGE_PRESENT;
        return;
    }

    // Everything seems ok, get the page (lazy allocation)
    Page_Table_Argumments arg = {};
    arg.user_access = 1;
    arg.writeable = 1;
    arg.execution_disabled = 0;
    u64 result = alloc_page_lazy(virtual_addr, arg, 0);

    // Return the result (success or failure)
    syscall_ret_low(task) = result;
    return;
}

void release_page(u64 virtual_addr)
{
    const task_ptr& task = get_current_task();

    // Check allignment to 4096K (page size)
    if (virtual_addr & 0xfff) {
        syscall_ret_low(task) = ERROR_UNALLIGNED;
        return;
    }

    // Check that program is not hijacking kernel space
    if (virtual_addr >= KERNEL_ADDR_SPACE) {
        syscall_ret_low(task) = ERROR_OUT_OF_RANGE;
        return;
    }

    // Check page
    Page_Types p = page_type(virtual_addr);
    if (p == Page_Types::UNALLOCATED) {
        syscall_ret_low(task) = ERROR_PAGE_NOT_ALLOCATED;
        return;
    }
    if (p != NORMAL) {
        syscall_ret_low(task) = ERROR_HUGE_PAGE;
        return;
    }

    // Everything ok, release the page
    syscall_ret_low(task) = release_page_s(virtual_addr, get_cpu_struct()->current_task->pid);
    return;
}

void getpid()
{
    const task_ptr& task = get_cpu_struct()->current_task;

    syscall_ret_low(task) = SUCCESS;
    syscall_ret_high(task) = task->pid;
}

void syscall_create_process()
{
    auto process = create_process(3);

    const task_ptr& task = get_current_task();

    if (process.result == SUCCESS) {
        syscall_ret_low(task) = SUCCESS;
        syscall_ret_high(task) = process.val->pid;
        return;
    }

    syscall_ret_low(task) = process.result;
    return;
}

void syscall_map_into()
{
    syscall_ret_low(get_current_task()) = ERROR_NOT_IMPLEMENTED;
}

void syscall_block(u64 mask)
{
    task_ptr task = get_current_task();
    ReturnStr<u64> r = block_task(get_cpu_struct()->current_task, mask);

    syscall_ret_low(task) = r.result;
    syscall_ret_high(task) = r.val;
    return;
}

void syscall_map_into_range(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    klib::shared_ptr<TaskDescriptor> current = get_cpu_struct()->current_task;

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
    if (page_start >= KERNEL_ADDR_SPACE or (page_start + nb_pages*KB(4)) > KERNEL_ADDR_SPACE or (page_start + nb_pages*KB(4) < page_start)) {
        syscall_ret_low(current) = ERROR_OUT_OF_RANGE;
        return;
    }

    // Check allignment
    if (page_start & 0xfff) {
        syscall_ret_low(current) = ERROR_UNALLIGNED; 
        return;
    }
    if (to_addr & 0xfff) {
        syscall_ret_low(current) = ERROR_UNALLIGNED; 
        return;
    }

    klib::shared_ptr<TaskDescriptor> t = get_task(pid);

    // Check if process exists
    if (not t) {
        syscall_ret_low(current) = ERROR_NO_SUCH_PROCESS;
        return;
    }

    Auto_Lock_Scope scope_lock(t->sched_lock);

    // Check process status
    if (not is_uninited(t)) {
        syscall_ret_low(current) = ERROR_PROCESS_INITED;
        return;
    }

    syscall_ret_low(current) = atomic_transfer_pages(current, t, page_start, to_addr, nb_pages, pta);
}

void syscall_share_with_range(u64 pid, u64 page_start, u64 to_addr, u64 nb_pages, u64 args)
{
    const task_ptr& task = get_current_task();

    // TODO: Check permissions

    // Check if legal address
    if (page_start >= KERNEL_ADDR_SPACE or (page_start + nb_pages*KB(4)) > KERNEL_ADDR_SPACE or (page_start + nb_pages*KB(4) < page_start)) {
        syscall_ret_low(task) = ERROR_OUT_OF_RANGE;
        return;
    }

    // Check allignment
    if (page_start & 0xfff) {
        syscall_ret_low(task) = ERROR_UNALLIGNED;
        return;
    }

    if (to_addr & 0xfff) {
        syscall_ret_low(task) = ERROR_UNALLIGNED;
        return;
    }

    klib::shared_ptr<TaskDescriptor> t = get_task(pid);

    // Check if process exists
    if (not t) {
        syscall_ret_low(task) = ERROR_NO_SUCH_PROCESS;
        return;
    }

    Auto_Lock_Scope scope_lock(t->sched_lock);

    Page_Table_Argumments pta = {};
    pta.user_access = 1;
    pta.global = 0;
    pta.writeable = args& FLAG_RW;
    pta.execution_disabled = args&FLAG_NOEXECUTE;

    kresult_t r = atomic_share_pages(t, page_start, to_addr, nb_pages, pta);

    syscall_ret_low(task) = r;
}

void syscall_get_page_multi(u64 virtual_addr, u64 nb_pages)
{
    const task_ptr& task = get_current_task();

    // Check allignment to 4096K (page size)
    if (virtual_addr & 0xfff) {
        syscall_ret_low(task) = ERROR_UNALLIGNED;
        return;
    }

    // Check that program is not hijacking kernel space
    if (virtual_addr >= KERNEL_ADDR_SPACE or (virtual_addr + nb_pages*KB(4)) > KERNEL_ADDR_SPACE or (virtual_addr + nb_pages*KB(4) < virtual_addr)) {
        syscall_ret_low(task) = ERROR_OUT_OF_RANGE;
        return;
    }

    // Everything seems ok, get the page (lazy allocation)
    Page_Table_Argumments arg = {};
    arg.user_access = 1;
    arg.writeable = 1;
    arg.execution_disabled = 1;
    u64 result = SUCCESS;
    u64 i = 0;

    Auto_Lock_Scope scope_lock_paging(get_cpu_struct()->current_task->page_table.shared_str->lock);

    for (; i < nb_pages and result == SUCCESS; ++i)
        result = alloc_page_lazy(virtual_addr + i*KB(4), arg, 0);

    // If unsuccessfull, return everything back
    if (result != SUCCESS)
        for (u64 k = 0; k < i; ++k) 
            invalidade_noerr(virtual_addr + k*KB(4));

    // Return the result (success or failure)
    syscall_ret_low(task) = result;
}

void syscall_release_page_multi(u64 virtual_addr, u64 nb_pages)
{
    const task_ptr& task = get_current_task();

    // Check allignment to 4096K (page size)
    if (virtual_addr & 0xfff) {
        syscall_ret_low(task) = ERROR_UNALLIGNED;
        return;
    }

    // Check that program is not hijacking kernel space
    if (virtual_addr >= KERNEL_ADDR_SPACE or (virtual_addr + nb_pages*KB(4)) > KERNEL_ADDR_SPACE or (virtual_addr + nb_pages*KB(4) < virtual_addr)) {
        syscall_ret_low(task) = ERROR_OUT_OF_RANGE;
        return;
    }

    Auto_Lock_Scope scope_lock_paging(get_cpu_struct()->current_task->page_table.get_lock());

    // Check pages
    for (u64 i = 0; i < nb_pages; ++i) {
        Page_Types p = page_type(virtual_addr);
        if (p == Page_Types::UNALLOCATED) {
            syscall_ret_low(task) = ERROR_PAGE_NOT_ALLOCATED;
            return;
        }
        if (p != NORMAL) {
            syscall_ret_low(task) = ERROR_HUGE_PAGE;
            return;
        }
    }

    kresult_t r = SUCCESS;

    // TODO: Leaves user process in unknown state afterwards

    // Everything ok, release pages
    for (u64 i = 0; i < nb_pages and r == SUCCESS; ++i)
        r = release_page_s(virtual_addr + i*KB(4), get_cpu_struct()->current_task->pid);

    // Return the result (success or failure)
    syscall_ret_low(task) = r;
}

void syscall_start_process(u64 pid, u64 start, u64 arg1, u64 arg2, u64 arg3)
{
    task_ptr task = get_current_task();
    // TODO: Check permissions

    klib::shared_ptr<TaskDescriptor> t = get_task(pid);

    // Check if process exists
    if (not t) {
        syscall_ret_low(task) = ERROR_NO_SUCH_PROCESS;
        return;
    }

    Auto_Lock_Scope scope_sched_lock(t->sched_lock);

    // Check process status
    if (not is_uninited(t)) {
        syscall_ret_low(task) = ERROR_PROCESS_INITED;
        return;
    }

    // Set entry
    t->set_entry_point(start);

    // Pass arguments
    t->regs.scratch_r.rdi = arg1;
    t->regs.scratch_r.rsi = arg2;
    t->regs.scratch_r.rdx = arg3;

    // Init task
    init_task(t);

    syscall_ret_low(task) = SUCCESS;
}

void syscall_init_stack(u64 pid, u64 esp)
{
    const task_ptr& task = get_current_task();

    ReturnStr<u64> r = {ERROR_GENERAL, 0};
    // TODO: Check permissions

    klib::shared_ptr<TaskDescriptor> t = get_task(pid);

    // Check if process exists
    if (not t) {
        syscall_ret_low(task) = ERROR_NO_SUCH_PROCESS;
        return;
    }

    Auto_Lock_Scope scope_sched_lock(t->sched_lock);

    // Check process status
    if (not is_uninited(t)) {
        syscall_ret_low(task) = ERROR_PROCESS_INITED;
        return;
    }

    if (esp == 0) { // If ESP == 0 use default kernel's stack policy
        r = t->init_stack();
    } else {
        t->regs.e.rsp = esp;
        r = {SUCCESS, esp};
    }

    syscall_ret_low(task) = r.result;
    syscall_ret_high(task) = r.val;
}

void syscall_exit(u64 arg1, u64 arg2)
{
    klib::shared_ptr<TaskDescriptor> task = get_cpu_struct()->current_task;

    // Record exit code
    task->ret_hi = arg2;
    task->ret_lo = arg1;

    // Kill the process
    kill(task);

    syscall_ret_low(task) = SUCCESS;
}

void syscall_map_phys(u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
    const task_ptr& task = get_current_task();

    u64 virt = arg1;
    u64 phys = arg2;
    u64 nb_pages = arg3;

    Page_Table_Argumments pta = {};
    pta.user_access = 1;
    pta.extra = 0b010;
    pta.global = 0;
    pta.writeable = arg4& 0x01;
    pta.execution_disabled = arg4&0x02;
    //t_print_bochs("Debug: map_phys virt %h <- phys %h nb %h pid %i\n", virt, phys, nb_pages, get_cpu_struct()->current_task->pid);

    // TODO: Check permissions

    // Check if legal address
    if (virt >= KERNEL_ADDR_SPACE or (virt + nb_pages*KB(4)) > KERNEL_ADDR_SPACE or (virt + nb_pages*KB(4) < virt)) {
        syscall_ret_low(task) = ERROR_OUT_OF_RANGE;
        return;
    }

    // Check allignment
    if ((virt & 0xfff) or (phys & 0xfff)) {
        syscall_ret_low(task) = ERROR_UNALLIGNED;
        return;
    }

    // TODO: Check if physical address is ok

    Auto_Lock_Scope paging_lock_scope(get_cpu_struct()->current_task->page_table.shared_str->lock);

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

    syscall_ret_low(task) =  r;
}

void syscall_get_first_message(u64 buff, u64 args)
{
    klib::shared_ptr<TaskDescriptor> current = get_cpu_struct()->current_task;
    kresult_t result = SUCCESS;
    klib::shared_ptr<Message> top_message;

    {
        Auto_Lock_Scope messaging_lock(current->messaging_lock);
        if (current->messages.empty()) {
            syscall_ret_low(current) = ERROR_NO_MESSAGES;
            return;
        }
        top_message = current->messages.front();
        if (!(args & MSG_ARG_NOPOP)) {
            current->messages.pop_front();
        }
    }

    result = top_message->copy_to_user_buff((char*)buff);

    if (result != SUCCESS and not (args & MSG_ARG_NOPOP)) {
        Auto_Lock_Scope messaging_lock(current->messaging_lock);
        current->messages.push_front(top_message);
    }

    syscall_ret_low(current) = result;
}

void syscall_send_message_task(u64 pid, u64 channel, u64 size, u64 message)
{
    klib::shared_ptr<TaskDescriptor> current = get_cpu_struct()->current_task;

    // TODO: Check permissions

    u64 self_pid = get_cpu_struct()->current_task->pid;
    if (self_pid == pid) {
        t_print_bochs("[Kernel] Warning: PID %h sent message to self\n", pid);
        syscall_ret_low(current) = ERROR_CANT_MESSAGE_SELF;
        return;
    }

    klib::shared_ptr<TaskDescriptor> t = get_task(pid);

    // Check if process exists
    if (not t) {
        syscall_ret_low(current) = ERROR_NO_SUCH_PROCESS;
        return;
    }

    kresult_t result = SUCCESS;

    klib::vector<char> msg(size);
    result = copy_from_user((char*)message, &msg.front(), size);

    if (result != SUCCESS) {
        syscall_ret_low(current) = result;
        return;
    }

    klib::shared_ptr<Message> ptr = klib::make_shared<Message>(Message({self_pid, channel, klib::move(msg)}));

    {
        Auto_Lock_Scope lock(t->messaging_lock);
        result = queue_message(t, klib::move(ptr));
    }

    unblock_if_needed(t, MESSAGE_S_NUM);

    syscall_ret_low(current) = result;
}

void syscall_send_message_port(u64 port, size_t size, u64 message)
{
    klib::shared_ptr<TaskDescriptor> current = get_cpu_struct()->current_task;

    // TODO: Check permissions

    kresult_t result = current->ports.send_from_user(current->pid, port, message, size);

    syscall_ret_low(current) = result;
}


void syscall_set_port(u64 pid, u64 port, u64 dest_pid, u64 dest_chan)
{
    const task_ptr& task = get_current_task();

    // TODO: Check permissions

    if (pid == dest_pid) {
        syscall_ret_low(task) = ERROR_CANT_MESSAGE_SELF;
        return;
    }

    klib::shared_ptr<TaskDescriptor> t = get_task(pid);

    // Check if process exists
    if (not t) {
        syscall_ret_low(task) = ERROR_NO_SUCH_PROCESS;
        return;
    }

    Auto_Lock_Scope messaging_lock(t->messaging_lock);
    kresult_t result = t->ports.set_port(port, t, dest_chan);

    syscall_ret_low(task) = result;
}

void syscall_set_port_kernel(u64 port, u64 dest_pid, u64 dest_chan)
{
    const task_ptr& task = get_current_task();

    // TODO: Check permissions

    klib::shared_ptr<TaskDescriptor> t = get_task(dest_pid);

    // Check if process exists
    if (not t) {
        syscall_ret_low(task) = ERROR_NO_SUCH_PROCESS;
        return;
    }

    messaging_ports.lock();
    kresult_t result = kernel_ports.set_port(port, t, dest_chan);
    messaging_ports.unlock();

    syscall_ret_low(task) = result;
}

void syscall_set_port_default(u64 port, u64 dest_pid, u64 dest_chan)
{
    const task_ptr& task = get_current_task();

    // TODO: Check permissions

    klib::shared_ptr<TaskDescriptor> t = get_task(dest_pid);

    // Check if process exists
    if (not t) {
        syscall_ret_low(task) = {ERROR_NO_SUCH_PROCESS};
        return;
    }

    messaging_ports.lock();
    kresult_t result = default_ports.set_port(port, t, dest_chan);
    messaging_ports.unlock();

    syscall_ret_low(task) = result;
}

void syscall_get_message_info(u64 message_struct)
{
    const task_ptr& task = get_current_task();

    // TODO: This creates race conditions

    klib::shared_ptr<TaskDescriptor> current = get_cpu_struct()->current_task;

    kresult_t result = SUCCESS;
    klib::shared_ptr<Message> msg;

    {
        Auto_Lock_Scope lock(current->messaging_lock);
        if (current->messages.empty()) {
            syscall_ret_low(task) = ERROR_NO_MESSAGES;
            return;
        }
        msg = current->messages.front();
    }

    u64 msg_struct_size = sizeof(Message_Descriptor);

    result = prepare_user_buff_wr((char*)message_struct, msg_struct_size);

    if (result == SUCCESS) {
        Message_Descriptor& desc = *(Message_Descriptor*)message_struct;
        desc.sender = msg->from;
        desc.channel = msg->channel;
        desc.size = msg->size();
    }

    syscall_ret_low(task) = result;
}

void syscall_set_attribute(u64 pid, u64 attribute, u64 value)
{
    task_ptr task = get_current_task();

    // TODO: Check persmissions

    klib::shared_ptr<TaskDescriptor> process = pid == task->pid ? task : get_task(pid);
    Interrupt_Stackframe* current_frame = &process->regs.e;

    // Check if process exists
    if (not process) {
        syscall_ret_low(task) = ERROR_NO_SUCH_PROCESS;
        return;
    }

    kresult_t result = ERROR_GENERAL;

    switch (attribute) {
    case ATTR_ALLOW_PORT:
        current_frame->rflags.bits.iopl = value ? 3 : 0;
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

    syscall_ret_low(task) = result;
}

void syscall_is_page_allocated(u64 virtual_addr)
{
    const klib::shared_ptr<TaskDescriptor>& current_task = get_cpu_struct()->current_task;

    // Check allignment to 4096K (page size)
    if (virtual_addr & 0xfff) {
        syscall_ret_low(current_task) = ERROR_UNALLIGNED;
        return;
    }

    // Check that program is not hijacking kernel space
    if (virtual_addr >= KERNEL_ADDR_SPACE) {
        syscall_ret_low(current_task) = ERROR_OUT_OF_RANGE;
        return;
    }

    Auto_Lock_Scope lock(current_task->page_table.shared_str->lock);
    Page_Types type = page_type(virtual_addr);
    bool is_allocated = type != Page_Types::UNALLOCATED and type != Page_Types::UNKNOWN;

    syscall_ret_low(current_task) = SUCCESS;
    syscall_ret_high(current_task) = is_allocated;
}

void syscall_configure_system(u64 type, u64 arg1, u64 arg2)
{
    task_ptr task = get_current_task();
    // TODO: Check permissions

    ReturnStr<u64> result = {ERROR_NOT_SUPPORTED, 0};

    switch (type) {
    case SYS_CONF_LAPIC:
        result = lapic_configure(arg1, arg2);
        break;
    case SYS_CONF_CPU:
        result = cpu_configure(arg1, arg2);
        break;
    case SYS_CONF_SLEEP10:
        pit_sleep_100us(arg1);
        result = {SUCCESS, 0};
        break;
    default:
        break;
    };

    syscall_ret_low(task) = result.result;
    syscall_ret_high(task) = result.val;
}

void syscall_set_priority(u64 priority)
{
    task_ptr current_task = get_current_task();

    if (priority >= sched_queues_levels) {
        syscall_ret_low(current_task) = ERROR_NOT_SUPPORTED;
        return;
    }

    Auto_Lock_Scope lock(current_task->sched_lock);

    current_task->priority = priority;

    // TODO: Changing priority to lower one

    syscall_ret_low(current_task) = SUCCESS;
}

void syscall_get_lapic_id()
{
    const task_ptr& task = get_current_task();

    syscall_ret_low(task) = SUCCESS;
    syscall_ret_high(task) = get_cpu_struct()->lapic_id;
}

void program_syscall()
{
    write_msr(0xC0000081, ((u64)(R0_CODE_SEGMENT) << 32) | ((u64)(R3_LEGACY_CODE_SEGMENT) << 48)); // STAR (segments for user and kernel code)
    write_msr(0xC0000082, (u64)&syscall_entry); // LSTAR (64 bit entry point)
    write_msr(0xC0000084, ~0x0); // SFMASK (mask for %rflags)

    // Enable SYSCALL/SYSRET in EFER register
    u64 efer = read_msr(0xC0000080);
    write_msr(0xC0000080, efer | (0x01 << 0));
}