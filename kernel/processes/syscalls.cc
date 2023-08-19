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
#include <exceptions.hh>
#include <lib/utility.hh>
#include <memory/mem_object.hh>
#include <dbg.h>
#include <assert.h>
#include "task_group.hh"

using syscall_function = void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
klib::array<syscall_function, 35> syscall_table = {
    syscall_exit,
    getpid,
    syscall_create_process,
    syscall_start_process,
    syscall_init_stack,
    syscall_set_priority,
    syscall_set_task_name,
    syscall_get_lapic_id,
    syscall_configure_system,

    syscall_get_message_info,
    syscall_get_first_message,
    syscall_request_named_port,
    syscall_send_message_port,
    syscall_create_port,
    syscall_set_attribute,
    syscall_set_interrupt,
    syscall_name_port,
    syscall_get_port_by_name,
    syscall_set_log_port,

    syscall_get_page_table,
    nullptr,
    syscall_transfer_region,
    syscall_create_normal_region,
    syscall_get_segment,
    syscall_create_phys_map_region,
    syscall_delete_region,
    nullptr,
    nullptr,
    syscall_set_segment,

    syscall_asign_page_table,
    nullptr,
    syscall_create_task_group,
    syscall_add_to_task_group,
    syscall_remove_from_task_group,
    syscall_is_in_task_group,
};

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

    //t_print_bochs("Debug: syscall %h pid %h (%s) \n", call_n, get_cpu_struct()->current_task->pid, get_cpu_struct()->current_task->name.c_str());
    //t_print_bochs(" %h %h %h %h %h ", arg1, arg2, arg3, arg4, arg5);
    if (task->attr.debug_syscalls) {
        global_logger.printf("Debug: syscall %h pid %h\n", call_n, get_cpu_struct()->current_task->pid);
    }

    try {
        if (call_n > syscall_table.size() or syscall_table[call_n] == nullptr)
            throw Kern_Exception(ERROR_NOT_SUPPORTED, "syscall is not supported");

        syscall_table[call_n](arg1, arg2, arg3, arg4, arg5, 0);
    } catch (Kern_Exception &e) {
        syscall_ret_low(task) = e.err_code;
        t_print_bochs(" -> %h (%s)\n", e.err_code, e.err_message);
        return;
    } catch (...) {
        t_print_bochs("[Kernel] Caught unknown exception\n");
        syscall_ret_low(task) = ERROR_GENERAL;
        return;
    }
    
    //t_print_bochs(" -> SUCCESS\n");
    syscall_ret_low(task) = SUCCESS;
}

void getpid(u64, u64, u64, u64, u64, u64)
{
    const task_ptr& task = get_cpu_struct()->current_task;

    syscall_ret_high(task) = task->pid;
}

void syscall_create_process(u64, u64, u64, u64, u64, u64)
{
    syscall_ret_high(get_current_task()) = TaskDescriptor::create_process(3)->pid;
}

void syscall_start_process(u64 pid, u64 start, u64 arg1, u64 arg2, u64 arg3, u64)
{
    task_ptr task = get_current_task();
    // TODO: Check permissions

    klib::shared_ptr<TaskDescriptor> t = get_task_throw(pid);

    Auto_Lock_Scope scope_sched_lock(t->sched_lock);

    // If the process is running, you can't start it again
    if (not t->is_uninited())
        throw(Kern_Exception(ERROR_PROCESS_INITED, "Process is not in UNINIT state"));

    // Set entry
    t->set_entry_point(start);

    // Pass arguments
    t->regs.scratch_r.rdi = arg1;
    t->regs.scratch_r.rsi = arg2;
    t->regs.scratch_r.rdx = arg3;

    // Init task
    t->init();
}

void syscall_init_stack(u64 pid, u64 esp, u64, u64, u64, u64)
{
    const task_ptr& task = get_current_task();

    // TODO: Check permissions

    klib::shared_ptr<TaskDescriptor> t = get_task_throw(pid);
    Auto_Lock_Scope scope_sched_lock(t->sched_lock);

    // If the process is running, you can't start it again
    if (not t->is_uninited())
        throw(Kern_Exception(ERROR_PROCESS_INITED, "Process is not in UNINIT state"));

    if (esp == 0) { // If ESP == 0 use default kernel's stack policy
        syscall_ret_high(task) = t->init_stack();
    } else {
        t->regs.e.rsp = esp;

        syscall_ret_high(task) = esp;
    }
}

void syscall_exit(u64 arg1, u64 arg2, u64, u64, u64, u64)
{
    klib::shared_ptr<TaskDescriptor> task = get_cpu_struct()->current_task;

    // Record exit code
    task->ret_hi = arg2;
    task->ret_lo = arg1;

    // Kill the process
    task->atomic_kill();
}

void syscall_get_first_message(u64 buff, u64 args, u64 portno, u64, u64, u64)
{
    klib::shared_ptr<TaskDescriptor> current = get_cpu_struct()->current_task;
    klib::shared_ptr<Port> port;

    // Optimization: Try last blocked-by port
    port = klib::dynamic_pointer_cast<Port>(current->blocked_by.lock());

    if (not port or port->portno != portno)
        port = global_ports.atomic_get_port_throw(portno);

    klib::shared_ptr<Message> top_message;

    {
        Auto_Lock_Scope scope_lock(port->lock);

        if (current != port->owner.lock())
            throw(Kern_Exception(ERROR_NO_PERMISSION, "Callee is not a port owner"));

        if (port->is_empty())
            throw(Kern_Exception(ERROR_NO_MESSAGES, "Port queue is empty"));

        top_message = port->get_front();

        bool result = top_message->copy_to_user_buff((char*)buff);

        if (not result)
            return;

        if (!(args & MSG_ARG_NOPOP)) {
            port->pop_front();
        }
    }
}

void syscall_send_message_port(u64 port_num, size_t size, u64 message, u64, u64, u64)
{
    klib::shared_ptr<TaskDescriptor> current = get_cpu_struct()->current_task;

    // TODO: Check permissions

    klib::shared_ptr<Port> port;

    // Optimization: Try hint
    port = klib::dynamic_pointer_cast<Port>(current->blocked_by.lock());

    if (not port or port->portno != port_num) {
        port = global_ports.atomic_get_port_throw(port_num);
    }

    port->atomic_send_from_user(current, (char *)message, size);
}

void syscall_get_message_info(u64 message_struct, u64 portno, u64 flags, u64, u64, u64)
{
    task_ptr task = get_current_task();

    klib::shared_ptr<Port> port;

    // Optimization: Try last blocked-by port
    port = klib::dynamic_pointer_cast<Port>(task->blocked_by.lock());

    if (not port or port->portno != portno) {
        port = global_ports.atomic_get_port_throw(portno);
    }

    klib::shared_ptr<Message> msg;

    {
        Auto_Lock_Scope lock(port->lock);
        if (port->is_empty()) {
            constexpr unsigned FLAG_NOBLOCK= 0x01;

            if (flags & FLAG_NOBLOCK) {
                throw(Kern_Exception(ERROR_NO_MESSAGES, "FLAG_NOBLOCK is set and the process has no messages"));

            } else {
                task->request_repeat_syscall();
                block_current_task(port);
                return;
            }
        }
        msg = port->get_front();
    }

    u64 msg_struct_size = sizeof(Message_Descriptor);

    bool result = prepare_user_buff_wr((char*)message_struct, msg_struct_size);
    if (not result)
        return;

    Message_Descriptor& desc = *(Message_Descriptor*)message_struct;
    desc.sender = msg->pid_from;
    desc.channel = 0;
    desc.size = msg->size();
}

void syscall_set_attribute(u64 pid, u64 attribute, u64 value, u64, u64, u64)
{
    task_ptr task = get_current_task();

    // TODO: Check persmissions

    klib::shared_ptr<TaskDescriptor> process = pid == task->pid ? task : get_task_throw(pid);
    Interrupt_Stackframe* current_frame = &process->regs.e;

    switch (attribute) {
    case ATTR_ALLOW_PORT:
        current_frame->rflags.bits.iopl = value ? 3 : 0;
        break;

    case ATTR_DEBUG_SYSCALLS:
        process->attr.debug_syscalls = value;
        break;

    default:
        throw(Kern_Exception(ERROR_NOT_IMPLEMENTED, "syscall_set_attribute with given request is not implemented"));
        break;
    }
}

void syscall_configure_system(u64 type, u64 arg1, u64 arg2, u64, u64, u64)
{
    task_ptr task = get_current_task();
    // TODO: Check permissions

    switch (type) {
    case SYS_CONF_LAPIC:
         syscall_ret_high(get_current_task()) = lapic_configure(arg1, arg2);
        break;

    case SYS_CONF_CPU:
        syscall_ret_high(get_current_task()) = cpu_configure(arg1, arg2);
        break;

    case SYS_CONF_SLEEP10:
        pit_sleep_100us(arg1);
        break;

    default:
        throw (Kern_Exception(ERROR_NOT_IMPLEMENTED, "syscall_configure_system with unknown parameter"));
        break;
    };
}

void syscall_set_priority(u64 priority, u64, u64, u64, u64, u64)
{
    task_ptr current_task = get_current_task();

    if (priority >= sched_queues_levels) {
        throw (Kern_Exception(ERROR_NOT_SUPPORTED, "priority outside of queue levels"));
    }

    Auto_Lock_Scope lock(current_task->sched_lock);

    current_task->priority = priority;

    reschedule();
}

void syscall_get_lapic_id(u64, u64, u64, u64, u64, u64)
{
    syscall_ret_high(get_current_task()) = get_cpu_struct()->lapic_id;
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

void syscall_set_task_name(u64 pid, u64 /* const char* */ string, u64 length, u64, u64, u64)
{
    const task_ptr& current = get_cpu_struct()->current_task;

    task_ptr t = get_task_throw(pid);

    auto str = klib::string::fill_from_user(reinterpret_cast<const char*>(string), length);

    if (not str.first) {
        return;
    }

    t->name.swap(str.second);
}

void syscall_create_port(u64 owner, u64, u64, u64, u64, u64)
{
    const task_ptr& task = get_current_task();

    // TODO: Check permissions

    klib::shared_ptr<TaskDescriptor> t;

    if (owner == 0 or task->pid == owner) {
        t = task;
    } else {
        t = get_task_throw(owner);
    }


    syscall_ret_high(task) = global_ports.atomic_request_port(t);
}

void syscall_set_interrupt(uint64_t port, u64 intno, u64 flags, u64, u64, u64)
{
    // t_print_bochs("syscall_set_interrupt(%i, %i, %i)\n", port, intno, flags);


    const task_ptr& task = get_current_task();

    klib::shared_ptr<Port> port_ptr = global_ports.atomic_get_port_throw(port);

    if (not intno_is_ok(intno))
        throw(Kern_Exception(ERROR_OUT_OF_RANGE, "intno outside of supported"));

    Prog_Int_Descriptor& desc = get_descriptor(prog_int_array, intno);

    { 
        Auto_Lock_Scope local_lock(desc.lock);
        desc.port = port_ptr;
    }
}

void syscall_name_port(u64 portnum, u64 /*const char* */ name, u64 length, u64 flags, u64, u64)
{
    const task_ptr& task = get_current_task();


    auto str = klib::string::fill_from_user(reinterpret_cast<const char*>(name), length);
    if (not str.first) {
        return;
    }

    klib::shared_ptr<Port> port = global_ports.atomic_get_port_throw(portnum);

    Auto_Lock_Scope scope_lock(port->lock);
    Auto_Lock_Scope scope_lock2(global_named_ports.lock);

    klib::shared_ptr<Named_Port_Desc> named_port = global_named_ports.storage.get_copy_or_default(str.second);

    if (named_port) {
        if (not named_port->parent_port.expired()) {
            throw(Kern_Exception(ERROR_NAME_EXISTS, "Named port with a given name already exists"));
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
}

void syscall_get_port_by_name(u64 /* const char * */ name, u64 length, u64 flags, u64, u64, u64)
{
    // --------------------- TODO: shared pointers *will* explode if the TaskDescriptor is deleted -------------------------------------
    constexpr unsigned flag_noblock = 0x01;

    task_ptr task = get_current_task();

    auto str = klib::string::fill_from_user(reinterpret_cast<const char*>(name), length);
    if (not str.first) {
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
                throw(Kern_Exception(ERROR_GENERAL, "named port parent is expired"));
            } else {
                syscall_ret_high(task) = ptr->portno;                
            }
            return;
        } else {
            if (flags & flag_noblock) {
                throw(Kern_Exception(ERROR_PORT_DOESNT_EXIST, "requested named port does not exist"));
                return;
            } else {
                named_port->actions.push_back(klib::make_unique<Notify_Task>(task, klib::shared_ptr<Generic_Port>(named_port)));

                task->request_repeat_syscall();
                block_current_task(named_port);
            }
        }
    } else {
        if (flags & flag_noblock) {
            throw(Kern_Exception(ERROR_PORT_DOESNT_EXIST, "requested named port does not exist"));
        } else {
            const klib::shared_ptr<Named_Port_Desc> new_desc = klib::make_shared<Named_Port_Desc>(klib::move(str.second), klib::shared_ptr<Port>(nullptr));

            global_named_ports.storage.insert({new_desc->name, new_desc});
            new_desc->actions.push_back(klib::make_unique<Notify_Task>(task, new_desc));

            task->request_repeat_syscall();
            block_current_task(new_desc);
        }
    }
}

void syscall_request_named_port(u64 string_ptr, u64 length, u64 reply_port, u64 flags, u64, u64)
{
    const task_ptr& task = get_current_task();

    auto str = klib::string::fill_from_user(reinterpret_cast<char*>(string_ptr), length);
    if (not str.first) {
        return;
    }

    klib::shared_ptr<Port> port_ptr = global_ports.atomic_get_port_throw(reply_port);

    Auto_Lock_Scope scope_lock(global_named_ports.lock);

    klib::shared_ptr<Named_Port_Desc> named_port = global_named_ports.storage.get_copy_or_default(str.second);

    if (named_port) {
        if (not named_port->parent_port.expired()) {
            klib::shared_ptr<Port> ptr = named_port->parent_port.lock();
            if (not ptr) {
                throw(Kern_Exception(ERROR_GENERAL, "named port parent is expired"));
            } else {
                Send_Message msg(port_ptr);
                msg.do_action(ptr, str.second);
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
}

void syscall_set_log_port(u64 port, u64 flags, u64, u64, u64, u64)
{
    const task_ptr& task = get_current_task();

    klib::shared_ptr<Port> port_ptr = global_ports.atomic_get_port_throw(port);

    global_logger.set_port(port_ptr, flags);
}

void syscall_create_normal_region(u64 pid, u64 addr_start, u64 size, u64 access, u64, u64)
{
    const klib::shared_ptr<TaskDescriptor>& current = get_cpu_struct()->current_task;

    klib::shared_ptr<TaskDescriptor> dest_task = nullptr;

    if (pid == 0 or current->pid == pid)
        dest_task = current;

    if (not dest_task)
        dest_task = get_task_throw(pid);

    // Syscall must be page aligned
    if (addr_start & 07777 or size & 07777) {
        throw(Kern_Exception(ERROR_UNALLIGNED, "arguments are not page aligned"));
        return;
    }

    syscall_ret_high(current) = dest_task->page_table->atomic_create_normal_region(addr_start, size, access & 0x07, access & 0x08, klib::string(), 0);
}

void syscall_create_phys_map_region(u64 pid, u64 addr_start, u64 size, u64 access, u64 phys_addr, u64)
{
    const klib::shared_ptr<TaskDescriptor>& current = get_cpu_struct()->current_task;

    klib::shared_ptr<TaskDescriptor> dest_task = nullptr;

    if (pid == 0 or current->pid == pid)
        dest_task = current;

    if (not dest_task)
        dest_task = get_task_throw(pid);

    // Syscall must be page aligned
    if (addr_start & 07777 or size & 07777) {
        throw(Kern_Exception(ERROR_UNALLIGNED, "arguments are not page aligned"));
        return;
    }

    syscall_ret_high(current) = dest_task->page_table->atomic_create_phys_region(addr_start, size, access & 0x07, access & 0x08, klib::string(), phys_addr);
}

void syscall_get_page_table(u64 pid, u64, u64, u64, u64, u64)
{
    const klib::shared_ptr<TaskDescriptor>& current = get_cpu_struct()->current_task;

    klib::shared_ptr<TaskDescriptor> target{};

    if (pid == 0 or current->pid == pid)
        target = current;
    else
        target = get_task_throw(pid);

    Auto_Lock_Scope target_lock(target->sched_lock);

    if (not target->page_table) {
        throw(Kern_Exception(ERROR_HAS_NO_PAGE_TABLE, "process has no page table"));
    }
    
    syscall_ret_high(current) = target->page_table->id;
}

void syscall_set_segment(u64 pid, u64 segment_type, u64 ptr, u64, u64, u64)
{
    const klib::shared_ptr<TaskDescriptor>& current = get_cpu_struct()->current_task;
    klib::shared_ptr<TaskDescriptor> target{};

    if (pid == 0 or current->pid == pid)
        target = current;
    else
        target = get_task_throw(pid);

    if (target == current)
        save_segments(target);

    switch (segment_type) {
    case 1:
        target->regs.seg.fs = ptr;
        break;
    case 2:
        target->regs.seg.gs = ptr;
        break;
    default:
        throw Kern_Exception(ERROR_OUT_OF_RANGE, "invalid segment in syscall_set_segment");
    }

    if (target == current)
        restore_segments(target);
}

void syscall_get_segment(u64 pid, u64 segment_type, u64, u64, u64, u64)
{
    const klib::shared_ptr<TaskDescriptor>& current = get_cpu_struct()->current_task;
    klib::shared_ptr<TaskDescriptor> target{};
    u64 segment = 0;

    if (pid == 0 or current->pid == pid)
        target = current;
    else
        target = get_task_throw(pid);

    if (target == current)
        save_segments(target);

    switch (segment_type) {
    case 1:
        segment = target->regs.seg.fs;
        break;
    case 2:
        segment = target->regs.seg.gs;
        break;
    default:
        throw Kern_Exception(ERROR_OUT_OF_RANGE, "invalid segment in syscall_set_segment");
    }

    syscall_ret_high(current) = segment;
}

void syscall_transfer_region(u64 to_page_table, u64 region, u64 dest, u64 flags, u64, u64)
{
    const klib::shared_ptr<TaskDescriptor>& current = get_current_task();
    
    klib::shared_ptr<Page_Table> pt = Page_Table::get_page_table_throw(to_page_table);

    bool fixed = flags & 0x08;

    const auto result = current->page_table->atomic_transfer_region(pt, region, dest, flags, fixed);

    syscall_ret_high(current) = result;
}

void syscall_asign_page_table(u64 pid, u64 page_table, u64 flags, u64, u64, u64)
{
    const klib::shared_ptr<TaskDescriptor>& current = get_current_task();
    const klib::shared_ptr<TaskDescriptor> dest = get_task_throw(pid);

    switch (flags) {
    case 1: // PAGE_TABLE_CREATE
        dest->create_new_page_table();
        syscall_ret_high(current) = dest->page_table->id;
        break;
    case 2: // PAGE_TABLE_ASIGN
        {
            klib::shared_ptr<Page_Table> t;
            if (page_table == 0 or page_table == current->page_table->id)
                t = current->page_table;
            else
                t = Page_Table::get_page_table_throw(page_table);

            dest->register_page_table(klib::forward<klib::shared_ptr<Page_Table>>(t));
            syscall_ret_high(current) = dest->page_table->id;
            break;
        }
    case 3: // PAGE_TABLE_CLONE
        {
            klib::shared_ptr<Page_Table> t = current->page_table->create_clone();
            dest->register_page_table(klib::forward<klib::shared_ptr<Page_Table>>(t));
            syscall_ret_high(current) = dest->page_table->id;
            break;
        }
    default:
        throw Kern_Exception(ERROR_NOT_SUPPORTED, "value of flags parameter is not supported");
    }
}

void syscall_create_mem_object(u64 size_bytes, u64, u64, u64, u64, u64)
{
    const auto &current_task = get_current_task();
    const auto &current_page_table = current_task->page_table;

    // TODO: Remove hard-coded page sizes

    // Syscall must be page aligned
    if (size_bytes & 07777)
        throw Kern_Exception(ERROR_UNALLIGNED, "arguments are not page aligned");

    const auto page_size_log = 12; // 2^12 = 4096 bytes
    const auto size_pages = size_bytes/4096;

    const auto ptr = Mem_Object::create(page_size_log, size_pages);

    current_page_table->atomic_pin_memory_object(ptr);

    syscall_ret_high(current_task) = ptr->get_id();
}

void syscall_delete_region(u64 region_start, u64, u64, u64, u64, u64)
{
    const auto &current_task = get_current_task();
    const auto &current_page_table = current_task->page_table;

    // TODO: Contemplate about adding a check to see if it's inside the kernels address space

    // TODO: This is completely untested and knowing me is probably utterly broken
    current_page_table->atomic_delete_region(region_start);
}

void syscall_remove_from_task_group(u64 pid, u64 group, u64, u64, u64, u64)
{
    const auto task = pid == 0 ? get_current_task() : get_task_throw(pid);

    const auto group_ptr = TaskGroup::get_task_group_throw(group);

    group_ptr->atomic_remove_task(task);
}

void syscall_create_task_group(u64, u64 , u64 , u64 , u64 , u64)
{
    const auto &current_task = get_current_task();
    const auto new_task_group = TaskGroup::create();

    new_task_group->atomic_register_task(current_task);

    syscall_ret_high(current_task) = new_task_group->get_id();
}

void syscall_is_in_task_group(u64 pid, u64 group, u64, u64, u64, u64)
{
    const u64 current_pid = pid == 0 ? get_current_task()->pid : pid;
    const auto group_ptr = TaskGroup::get_task_group_throw(group);

    const bool has_task = group_ptr->atomic_has_task(current_pid);
    syscall_ret_high(get_current_task()) = has_task;
}

void syscall_add_to_task_group(u64 pid, u64 group, u64, u64, u64, u64)
{
    const auto task = pid == 0 ? get_current_task() : get_task_throw(pid);

    const auto group_ptr = TaskGroup::get_task_group_throw(group);

    group_ptr->atomic_register_task(task);
}