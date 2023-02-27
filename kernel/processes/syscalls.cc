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
#include <lib/string.hh>
#include <interrupts/programmable_ints.hh>
#include <messaging/named_ports.hh>
#include <kern_logger/kern_logger.hh>

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

    //t_print_bochs("Debug: syscall %h pid %h (%s)", call_n, get_cpu_struct()->current_task->pid, get_cpu_struct()->current_task->name.c_str());
    //t_print_bochs(" %h %h %h %h %h ", arg1, arg2, arg3, arg4, arg5);
    if (task->attr.debug_syscalls) {
        global_logger.printf("Debug: syscall %h pid %h\n", call_n, get_cpu_struct()->current_task->pid);
    }

    switch (call_n) {
    case SYSCALL_GETPID:
        getpid();
        break;
    case SYSCALL_CREATE_PROCESS:
        syscall_create_process();
        break;
    case SYSCALL_START_PROCESS:
        syscall_start_process(arg1, arg2, arg3, arg4, arg5);
        break;
    case SYSCALL_EXIT:
        syscall_exit(arg1, arg2);
        break;
    case SYSCALL_GET_MSG_INFO:
        syscall_get_message_info(arg1, arg2, arg3);
        break;
    case SYSCALL_GET_MESSAGE:
        syscall_get_first_message(arg1, arg2, arg3);
        break;
    case SYSCALL_SEND_MSG_PORT:
        syscall_send_message_port(arg1, arg2, arg3);
        break;
    case SYSCALL_SET_PORT:
        syscall_set_port(arg1, arg2, arg3, arg4);
        break;
    case SYSCALL_SET_ATTR:
        syscall_set_attribute(arg1, arg2, arg3);
        break;
    case SYSCALL_INIT_STACK:
        syscall_init_stack(arg1, arg2);
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
    case SYSCALL_SET_TASK_NAME:
        syscall_set_task_name(arg1, (char*)arg2, arg3);
        break;
    case SYSCALL_CREATE_PORT:
        syscall_create_port(arg1);
        break;
    case SYSCALL_SET_INTERRUPT:
        syscall_set_interrupt(arg1, arg2, arg3);
        break;
    case SYSCALL_NAME_PORT:
        syscall_name_port(arg1, (const char *)arg2, arg3, arg4);
        break;
    case SYSCALL_GET_PORT_BY_NAME:
        syscall_get_port_by_name((const char *)arg1, arg2, arg3);
        break;
    case SYSCALL_SET_LOG_PORT:
        syscall_set_log_port(arg1, arg2);
        break;
    case SYSCALL_REQUEST_NAMED_PORT:
        syscall_request_named_port(arg1, arg2, arg3, arg4);
        break;
    case SYSCALL_CREATE_NORMAL_REGION:
        syscall_create_normal_region(arg1, arg2, arg3, arg4);
        break;
    case SYSCALL_CREATE_MANAGED_REGION:
        syscall_create_managed_region(arg1, arg2, arg3, arg4, arg5);
        break;
    case SYSCALL_CREATE_PHYS_REGION:
        syscall_create_phys_map_region(arg1, arg2, arg3, arg4, arg5);
        break;
    default:
        // Not supported
        syscall_ret_low(task) = ERROR_NOT_SUPPORTED;
        break;
    }
    //t_print_bochs(" -> result %h\n", syscall_ret_low(task));
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

void syscall_get_first_message(u64 buff, u64 args, u64 portno)
{
    klib::shared_ptr<TaskDescriptor> current = get_cpu_struct()->current_task;
    kresult_t result = SUCCESS;

    klib::shared_ptr<Port> port;

    // Optimization: Try last blocked-by port
    port = klib::dynamic_pointer_cast<Port>(current->blocked_by.lock());

    if (not port or port->portno != portno) {
        port = global_ports.atomic_get_port(portno);
    }

    if (not port) {
        syscall_ret_low(current) = ERROR_PORT_DOESNT_EXIST;
        return;
    }

    klib::shared_ptr<Message> top_message;

    {
        Auto_Lock_Scope scope_lock(port->lock);
        if (current != port->owner.lock()) {
            syscall_ret_low(current) = ERROR_NO_PERMISSION;
            return;
        }

        if (port->msg_queue.empty()) {
            syscall_ret_low(current) = ERROR_NO_MESSAGES;
            return;
        }

        top_message = port->msg_queue.front();

        result = top_message->copy_to_user_buff((char*)buff);

        if (result != SUCCESS) {
            syscall_ret_low(current) = result;
            return;
        }

        if (!(args & MSG_ARG_NOPOP)) {
            port->msg_queue.pop_front();
        }
    }

    syscall_ret_low(current) = result;
}

void syscall_send_message_port(u64 port_num, size_t size, u64 message)
{
    klib::shared_ptr<TaskDescriptor> current = get_cpu_struct()->current_task;

    // TODO: Check permissions

    klib::shared_ptr<Port> port;

    // Optimization: Try hint
    port = klib::dynamic_pointer_cast<Port>(current->blocked_by.lock());

    if (not port or port->portno != port_num) {
        port = global_ports.atomic_get_port(port_num);
    }

    if (not port) {
        syscall_ret_low(current) = ERROR_PORT_DOESNT_EXIST;
        return;
    }

    kresult_t result = port->atomic_send_from_user(current, (char *)message, size);

    syscall_ret_low(current) = result;
}


void syscall_set_port(u64 pid, u64 port, u64 dest_pid, u64 dest_chan)
{
    const task_ptr& task = get_current_task();

    syscall_ret_low(task) = ERROR_NOT_IMPLEMENTED;
}

void syscall_get_message_info(u64 message_struct, u64 portno, u32 flags)
{
    task_ptr task = get_current_task();

    klib::shared_ptr<Port> port;

    // Optimization: Try last blocked-by port
    port = klib::dynamic_pointer_cast<Port>(task->blocked_by.lock());

    if (not port or port->portno != portno) {
        port = global_ports.atomic_get_port(portno);
    }

    if (not port) {
        syscall_ret_low(task) = ERROR_PORT_DOESNT_EXIST;
        return;
    }

    kresult_t result = SUCCESS;
    klib::shared_ptr<Message> msg;

    {
        Auto_Lock_Scope lock(port->lock);
        if (port->msg_queue.empty()) {
            constexpr unsigned FLAG_NOBLOCK= 0x01;

            if (flags & FLAG_NOBLOCK) {
                syscall_ret_low(task) = ERROR_NO_MESSAGES;
            } else {
                request_repeat_syscall(task);
                block_current_task(port);
            }

            return;
        }
        msg = port->msg_queue.front();
    }

    u64 msg_struct_size = sizeof(Message_Descriptor);

    result = prepare_user_buff_wr((char*)message_struct, msg_struct_size);

    if (result == SUCCESS) {
        Message_Descriptor& desc = *(Message_Descriptor*)message_struct;
        desc.sender = msg->pid_from;
        desc.channel = 0;
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


    write_msr(0x174, R0_CODE_SEGMENT); // IA32_SYSENTER_CS
    write_msr(0x175, (u64)get_cpu_struct()->kernel_stack_top); // IA32_SYSENTER_ESP
    write_msr(0x176, (u64)&sysenter_entry); // IA32_SYSENTER_EIP
}

void syscall_set_task_name(u64 pid, const char* string, u64 length)
{
    const task_ptr& current = get_cpu_struct()->current_task;

    task_ptr t = get_task(pid);

    // Check if process exists
    if (not t) {
        syscall_ret_low(current) = ERROR_NO_SUCH_PROCESS;
        return;
    }

    auto str = klib::string::fill_from_user(string, length);

    if (str.first != SUCCESS) {
        syscall_ret_low(current) = str.first;
        return;
    }

    t->name.swap(str.second);
    syscall_ret_low(current) = SUCCESS;
    return;
}

void syscall_create_port(u64 owner)
{
    const task_ptr& task = get_current_task();

    // TODO: Check permissions

    klib::shared_ptr<TaskDescriptor> t;

    if (owner == 0 or task->pid == owner) {
        t = task;
    } else {
        t = get_task(owner);
    }

    // Check if process exists
    if (not t) {
        syscall_ret_low(task) = ERROR_NO_SUCH_PROCESS;
        return;
    }


    auto result = global_ports.atomic_request_port(t);

    syscall_ret_low(task) = result.result;
    syscall_ret_high(task) = result.val;
}

void syscall_set_interrupt(uint64_t port, uint32_t intno, uint32_t flags)
{
    // t_print_bochs("syscall_set_interrupt(%i, %i, %i)\n", port, intno, flags);


    const task_ptr& task = get_current_task();

    klib::shared_ptr<Port> port_ptr = global_ports.atomic_get_port(port);

    if (not port_ptr) {
        syscall_ret_low(task) = ERROR_PORT_DOESNT_EXIST;
        return;
    }

    if (not intno_is_ok(intno)) {
        syscall_ret_low(task) = ERROR_OUT_OF_RANGE;
        return;
    }

    Prog_Int_Descriptor& desc = get_descriptor(prog_int_array, intno);

    { 
        Auto_Lock_Scope local_lock(desc.lock);
        desc.port = port_ptr;
    }

    syscall_ret_low(task) = SUCCESS;
    return;
}

void syscall_name_port(u64 portnum, const char* name, u64 length, u32 flags)
{
    const task_ptr& task = get_current_task();


    auto str = klib::string::fill_from_user(name, length);
    if (str.first != SUCCESS) {
        syscall_ret_low(task) = str.first;
        return;
    }

    klib::shared_ptr<Port> port = global_ports.atomic_get_port(portnum);

    if (not port) {
        syscall_ret_low(task) = ERROR_PORT_DOESNT_EXIST;
        return;
    }

    Auto_Lock_Scope scope_lock(port->lock);

    Auto_Lock_Scope scope_lock2(global_named_ports.lock);

    klib::shared_ptr<Named_Port_Desc> named_port = global_named_ports.storage.get_copy_or_default(str.second);

    if (named_port) {
        if (not named_port->parent_port.expired()) {
            syscall_ret_low(task) = ERROR_NAME_EXISTS;
            return;
        }

        named_port->parent_port = port;

        for (const auto& t : named_port->actions) {
            t->do_action(port, str.second);
        }

        named_port->actions.clear();
    } else {
        const klib::shared_ptr<Named_Port_Desc> new_desc = klib::make_shared<Named_Port_Desc>(klib::move(str.second), port);

        global_named_ports.storage.insert({new_desc->name, new_desc});
    }

    syscall_ret_low(task) = SUCCESS;
    return;
}

void syscall_get_port_by_name(const char *name, u64 length, u32 flags)
{
    // --------------------- TODO: shared pointers *will* explode if the TaskDescriptor is deleted -------------------------------------
    constexpr unsigned flag_noblock = 0x01;

    task_ptr task = get_current_task();

    auto str = klib::string::fill_from_user(name, length);
    if (str.first != SUCCESS) {
        syscall_ret_low(task) = str.first;
        return;
    }

    Auto_Lock_Scope scope_lock(global_named_ports.lock);

    klib::shared_ptr<Named_Port_Desc> named_port;

    // Optimization: Try last blocked-by port
    named_port = klib::dynamic_pointer_cast<Named_Port_Desc>(task->blocked_by.lock());

    if (not named_port or named_port->name != str.second) {
        named_port = global_named_ports.storage.get_copy_or_default(str.second);
    }

    if (named_port) {
        if (not named_port->parent_port.expired()) {
            klib::shared_ptr<Port> ptr = named_port->parent_port.lock();
            if (not ptr) {
                syscall_ret_low(task) = ERROR_GENERAL;
            } else {
                syscall_ret_low(task) = SUCCESS;
                syscall_ret_high(task) = ptr->portno;                
            }
            return;
        } else {
            if (flags & flag_noblock) {
                syscall_ret_low(task) = ERROR_PORT_DOESNT_EXIST;
                return;
            } else {
                named_port->actions.push_back(klib::make_unique<Notify_Task>(task, klib::shared_ptr<Generic_Port>(named_port)));

                request_repeat_syscall(task);
                block_current_task(named_port);
            }
        }
    } else {
        if (flags & flag_noblock) {
            syscall_ret_low(task) = ERROR_PORT_DOESNT_EXIST;
            return;
        } else {
            const klib::shared_ptr<Named_Port_Desc> new_desc = klib::make_shared<Named_Port_Desc>(klib::move(str.second), klib::shared_ptr<Port>(nullptr));

            global_named_ports.storage.insert({new_desc->name, new_desc});
            new_desc->actions.push_back(klib::make_unique<Notify_Task>(task, new_desc));

            request_repeat_syscall(task);
            block_current_task(new_desc);
        }
    }

    syscall_ret_low(task) = SUCCESS;
    return;
}

void syscall_request_named_port(u64 string_ptr, u64 length, u64 reply_port, u32 flags)
{
    const task_ptr& task = get_current_task();

    auto str = klib::string::fill_from_user(reinterpret_cast<char*>(string_ptr), length);
    if (str.first != SUCCESS) {
        syscall_ret_low(task) = str.first;
        return;
    }

    klib::shared_ptr<Port> port_ptr = global_ports.atomic_get_port(reply_port);

    if (not port_ptr) {
        syscall_ret_low(task) = ERROR_PORT_DOESNT_EXIST;
        return;
    }

    Auto_Lock_Scope scope_lock(global_named_ports.lock);

    klib::shared_ptr<Named_Port_Desc> named_port = global_named_ports.storage.get_copy_or_default(str.second);

    if (named_port) {
        if (not named_port->parent_port.expired()) {
            klib::shared_ptr<Port> ptr = named_port->parent_port.lock();
            if (not ptr) {
                syscall_ret_low(task) = ERROR_GENERAL;
            } else {
                Send_Message msg(port_ptr);
                syscall_ret_low(task) = msg.do_action(ptr, str.second);
            }
            return;
        } else {
            named_port->actions.push_back(klib::make_unique<Send_Message>(port_ptr));
        }
    } else {
        const klib::shared_ptr<Named_Port_Desc> new_desc = klib::make_shared<Named_Port_Desc>( Named_Port_Desc(klib::move(str.second), nullptr) );

        auto it = global_named_ports.storage.insert({new_desc->name, new_desc});
        it.first->second->actions.push_back(klib::make_unique<Send_Message>(port_ptr));
    }

    syscall_ret_low(task) = SUCCESS;
    return;
}

void syscall_set_log_port(u64 port, u32 flags)
{
    const task_ptr& task = get_current_task();

    klib::shared_ptr<Port> port_ptr = global_ports.atomic_get_port(port);

    if (not port_ptr) {
        syscall_ret_low(task) = ERROR_PORT_DOESNT_EXIST;
        return;
    }

    syscall_ret_low(task) = global_logger.set_port(port_ptr, flags);
    return;
}

void syscall_create_normal_region(u64 pid, u64 addr_start, u64 size, u64 access)
{
    const klib::shared_ptr<TaskDescriptor>& current = get_cpu_struct()->current_task;

    klib::shared_ptr<TaskDescriptor> dest_task = nullptr;

    if (pid == 0 or current->pid == pid)
        dest_task = current;

    if (not dest_task)
        dest_task = get_task(pid);

    // Check if process exists
    if (not dest_task) {
        syscall_ret_low(current) = ERROR_NO_SUCH_PROCESS;
        return;
    }

    if (addr_start & 07777 or size & 07777) {
        syscall_ret_low(current) = ERROR_PAGE_NOT_ALLOCATED;
        return;
    }

    {
        Auto_Lock_Scope paging_lock(dest_task->page_table->lock);

        auto result = dest_task->page_table->create_normal_region(addr_start, size, access & 0x07, access & 0x08, klib::string(), 0);
        syscall_ret_low(current) = result.result;
        syscall_ret_high(current) = result.val;
        return;
    }
}

void syscall_create_managed_region(u64 pid, u64 addr_start, u64 size, u64 access, u64 port)
{
    const klib::shared_ptr<TaskDescriptor>& current = get_cpu_struct()->current_task;

    klib::shared_ptr<TaskDescriptor> dest_task = nullptr;

    if (pid == 0 or current->pid == pid)
        dest_task = current;

    if (not dest_task)
        dest_task = get_task(pid);

    // Check if process exists
    if (not dest_task) {
        syscall_ret_low(current) = ERROR_NO_SUCH_PROCESS;
        return;
    }

    if (addr_start & 07777 or size & 07777) {
        syscall_ret_low(current) = ERROR_PAGE_NOT_ALLOCATED;
        return;
    }

    klib::shared_ptr<Port> p = global_ports.atomic_get_port(port);

    if (not p) {
        syscall_ret_low(current) = ERROR_PORT_DOESNT_EXIST;
        return;
    }

    {
        Auto_Lock_Scope paging_lock(dest_task->page_table->lock);

        auto result = dest_task->page_table->create_managed_region(addr_start, size, access & 0x07, access & 0x08, klib::string(), klib::move(p));
        syscall_ret_low(current) = result.result;
        syscall_ret_high(current) = result.val;
        return;
    }
}

void syscall_create_phys_map_region(u64 pid, u64 addr_start, u64 size, u64 access, u64 phys_addr)
{
    const klib::shared_ptr<TaskDescriptor>& current = get_cpu_struct()->current_task;

    klib::shared_ptr<TaskDescriptor> dest_task = nullptr;

    if (pid == 0 or current->pid == pid)
        dest_task = current;

    if (not dest_task)
        dest_task = get_task(pid);

    // Check if process exists
    if (not dest_task) {
        syscall_ret_low(current) = ERROR_NO_SUCH_PROCESS;
        return;
    }

    if (addr_start & 07777 or size & 07777) {
        syscall_ret_low(current) = ERROR_PAGE_NOT_ALLOCATED;
        return;
    }

    {
        Auto_Lock_Scope paging_lock(dest_task->page_table->lock);

        auto result = dest_task->page_table->create_phys_region(addr_start, size, access & 0x07, access & 0x08, klib::string(), phys_addr);
        syscall_ret_low(current) = result.result;
        syscall_ret_high(current) = result.val;
        return;
    }
}

void syscall_get_page_table(u64 pid)
{
    const klib::shared_ptr<TaskDescriptor>& current = get_cpu_struct()->current_task;

    klib::shared_ptr<TaskDescriptor> target{};

    if (pid == 0 or current->pid == pid)
        target = current;
    else
        target = get_task(pid);

    if (not target) {
        syscall_ret_low(current) = ERROR_NO_SUCH_PROCESS;
        return;
    }

    Auto_Lock_Scope target_lock(target->sched_lock);

    if (not target->page_table) {
        syscall_ret_low(current) = ERROR_HAS_NO_PAGE_TABLE;
        return;
    }

    syscall_ret_low(current) = SUCCESS;
    syscall_ret_high(current) = target->page_table->id;
}