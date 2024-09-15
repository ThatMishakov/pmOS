/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <kernel/attributes.h>
#include <kernel/block.h>
#include <kernel/com.h>
#include <kernel/flags.h>
#include <kernel/messaging.h>
#include <kernel/sysinfo.h>
#include <lib/vector.hh>
#include <memory/paging.hh>
#include <messaging/messaging.hh>
#include <processes/syscalls.hh>
#include <sched/sched.hh>
#include <utils.hh>
// #include <cpus/cpus.hh>
#include "task_group.hh"

#include <assert.h>
#include <clock.hh>
#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>
#include <lib/string.hh>
#include <lib/utility.hh>
#include <memory/mem_object.hh>
#include <messaging/named_ports.hh>
#include <pmos/system.h>
#include <sched/sched.hh>

#ifdef __x86_64__
    #include <sched/segments.hh>
#endif

using syscall_function = void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
klib::array<syscall_function, 49> syscall_table = {
    syscall_exit,
    get_task_id,
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
    syscall_unmap_range,
    nullptr,
    syscall_set_segment,

    syscall_asign_page_table,
    nullptr,
    syscall_create_task_group,
    syscall_add_to_task_group,
    syscall_remove_from_task_group,
    syscall_is_in_task_group,

    syscall_set_notify_mask,
    syscall_load_executable,
    syscall_request_timer,
    syscall_set_affinity,
    syscall_complete_interrupt,
    syscall_yield,
    syscall_map_mem_object,
    nullptr,
    syscall_get_time,
    syscall_system_info,
    syscall_kill_task,
    syscall_pause_task,
    syscall_resume_task,
    syscall_get_page_address,
};

extern "C" void syscall_handler()
{
    TaskDescriptor *task = get_cpu_struct()->current_task;
    u64 call_n           = task->regs.syscall_number();
    u64 arg1             = syscall_arg1(task);
    u64 arg2             = syscall_arg2(task);
    u64 arg3             = syscall_arg3(task);
    u64 arg4             = syscall_arg4(task);
    u64 arg5             = syscall_arg5(task);

    // serial_logger.printf("syscall_handler: task: %d (%s) call_n: %x, arg1: %x, arg2: %x, arg3:
    // %x, arg4: %x, arg5: %x\n", task->task_id, task->name.c_str(), call_n, arg1, arg2, arg3, arg4,
    // arg5);

    // TODO: check permissions

    // t_print_bochs("Debug: syscall %h pid %h (%s) ", call_n,
    // get_cpu_struct()->current_task->task_id, get_cpu_struct()->current_task->name.c_str());
    // t_print_bochs(" %h %h %h %h %h ", arg1, arg2, arg3, arg4, arg5);
    if (task->attr.debug_syscalls) {
        global_logger.printf("Debug: syscall %h pid %h\n", call_n,
                             get_cpu_struct()->current_task->task_id);
    }

    try {
        // TODO: This crashes if the syscall is not implemented
        if (call_n > syscall_table.size() or syscall_table[call_n] == nullptr)
            throw Kern_Exception(-ENOTSUP, "syscall is not supported");

        syscall_table[call_n](arg1, arg2, arg3, arg4, arg5, 0);
    } catch (Kern_Exception &e) {
        syscall_ret_low(task) = e.err_code;
        t_print_bochs("Debug: syscall %h pid %h (%s) ", call_n, task->task_id, task->name.c_str());
        t_print_bochs(" -> %h (%s)\n", e.err_code, e.err_message);
        task->pop_repeat_syscall();
        return;
    } catch (...) {
        t_print_bochs("[Kernel] Caught unknown exception\n");
        syscall_ret_low(task) = -ENOTSUP;
        return;
    }

    // t_print_bochs("SUCCESS %h\n", syscall_ret_high(task));

    // t_print_bochs(" -> SUCCESS\n");
}

void get_task_id(u64, u64, u64, u64, u64, u64)
{
    const task_ptr &task = get_cpu_struct()->current_task;

    syscall_ret_low(task)  = SUCCESS;
    syscall_ret_high(task) = task->task_id;
}

void syscall_create_process(u64, u64, u64, u64, u64, u64)
{
    auto task             = get_current_task();
    syscall_ret_low(task) = SUCCESS;
    syscall_ret_high(task) =
        TaskDescriptor::create_process(TaskDescriptor::PrivilegeLevel::User)->task_id;
}

void syscall_start_process(u64 pid, u64 start, u64 arg1, u64 arg2, u64 arg3, u64)
{
    task_ptr task = get_current_task();
    // TODO: Check permissions

    TaskDescriptor *t = get_task_throw(pid);

    Auto_Lock_Scope scope_sched_lock(t->sched_lock);

    // If the process is running, you can't start it again
    if (not t->is_uninited())
        throw(Kern_Exception(-EINVAL, "Process is not in UNINIT state"));

    // Set entry
    t->set_entry_point(start);

    // Pass arguments
    t->regs.arg1() = arg1;
    t->regs.arg2() = arg2;
    t->regs.arg3() = arg3;

    syscall_ret_low(task) = SUCCESS;

    // Init task
    t->init();
}

void syscall_load_executable(u64 task_id, u64 object_id, u64 flags, u64 /* unused */,
                             u64 /* unused */, u64 /* unused */)
{
    task_ptr task = get_current_task();
    // TODO: Check permissions

    TaskDescriptor *t = get_task_throw(task_id);

    klib::shared_ptr<Mem_Object> object = Mem_Object::get_object(object_id);

    klib::string name;
    {
        Auto_Lock_Scope scope_lock(t->name_lock);
        name = t->name;
    }

    auto b = t->load_elf(object, name);
    // Blocking is not implemented. Return error
    if (!b)
        throw Kern_Exception(-ENOSYS, "pages not available immediately");

    // TODO: This will race once blocking is to be implemented
    syscall_ret_low(task) = SUCCESS;
}

void syscall_init_stack(u64 pid, u64 esp, u64, u64, u64, u64)
{
    const task_ptr &task = get_current_task();

    // TODO: Check permissions

    TaskDescriptor *t = get_task_throw(pid);
    Auto_Lock_Scope scope_sched_lock(t->sched_lock);

    // If the process is running, you can't start it again
    if (not t->is_uninited())
        throw(Kern_Exception(-EINVAL, "Process is not in UNINIT state"));

    if (esp == 0) { // If ESP == 0 use default kernel's stack policy
        syscall_ret_high(task) = t->init_stack();
    } else {
        t->regs.stack_pointer() = esp;

        syscall_ret_low(task)  = SUCCESS;
        syscall_ret_high(task) = esp;
    }
}

void syscall_exit(u64 arg1, u64 arg2, u64, u64, u64, u64)
{
    TaskDescriptor *task = get_cpu_struct()->current_task;

    // Record exit code
    task->ret_hi = arg2;
    task->ret_lo = arg1;

    syscall_ret_low(task) = SUCCESS;
    // Kill the process
    task->atomic_kill();
}

void syscall_kill_task(u64 task_id, u64, u64, u64, u64, u64)
{
    TaskDescriptor *task = get_current_task();
    TaskDescriptor *t    = get_task_throw(task_id);

    syscall_ret_low(task) = SUCCESS;
    t->atomic_kill();
}

void syscall_get_first_message(u64 buff, u64 args, u64 portno, u64, u64, u64)
{
    TaskDescriptor *current = get_cpu_struct()->current_task;

    auto port = Port::atomic_get_port(portno);
    if (!port)
        throw Kern_Exception(-ENOENT, "port not found");

    klib::shared_ptr<Message> top_message;

    {
        Auto_Lock_Scope scope_lock(port->lock);

        if (current != port->owner)
            throw Kern_Exception(-EPERM, "Caller is not a port owner");

        if (port->is_empty())
            throw(Kern_Exception(-EAGAIN, "Port queue is empty"));

        top_message = port->get_front();

        bool result = top_message->copy_to_user_buff((char *)buff);

        if (not result)
            return;

        if (!(args & MSG_ARG_NOPOP)) {
            port->pop_front();
        }

        syscall_ret_low(current) = SUCCESS;
    }
}

void syscall_send_message_port(u64 port_num, size_t size, u64 message, u64, u64, u64)
{
    TaskDescriptor *current = get_cpu_struct()->current_task;

    // TODO: Check permissions

    auto port = Port::atomic_get_port(port_num);
    if (!port)
        throw Kern_Exception(-ENOENT, "port not found");

    syscall_ret_low(current) = SUCCESS;

    port->atomic_send_from_user(current, (char *)message, size);
}

void syscall_get_message_info(u64 message_struct, u64 portno, u64 flags, u64, u64, u64)
{
    task_ptr task = get_current_task();

    auto port = Port::atomic_get_port(portno);
    if (!port)
        throw Kern_Exception(-ENOENT, "port not found");

    if (port->owner != task) {
        throw(Kern_Exception(-EPERM, "Caller is not a port owner"));
    }

    klib::shared_ptr<Message> msg;

    {
        Auto_Lock_Scope lock(port->lock);
        if (port->is_empty()) {
            constexpr unsigned FLAG_NOBLOCK = 0x01;

            if (flags & FLAG_NOBLOCK) {
                throw(
                    Kern_Exception(-EAGAIN, "FLAG_NOBLOCK is set and the process has no messages"));

            } else {
                task->request_repeat_syscall();
                block_current_task(port);
                return;
            }
        }
        msg = port->get_front();
    }

    u64 msg_struct_size     = sizeof(Message_Descriptor);
    Message_Descriptor desc = {
        .sender  = msg->task_id_from,
        .channel = 0,
        .size    = msg->size(),
    };

    bool b = copy_to_user((char *)&desc, (char *)message_struct, msg_struct_size);
    if (not b)
        assert(!"blocking by page is not yet implemented");

    syscall_ret_low(task) = SUCCESS;
}

void syscall_set_attribute(u64 pid, u64 attribute, u64 value, u64, u64, u64)
{
    task_ptr task = get_current_task();

    // TODO: Check persmissions

    // TODO: This is *very* x86-specific and is a bad idea in general

#ifdef __x86_64__
    TaskDescriptor *process             = pid == task->task_id ? task : get_task_throw(pid);
    Interrupt_Stackframe *current_frame = &process->regs.e;

    switch (attribute) {
    case ATTR_ALLOW_PORT:
        current_frame->rflags.bits.iopl = value ? 3 : 0;
        break;

    case ATTR_DEBUG_SYSCALLS:
        process->attr.debug_syscalls = value;
        break;

    default:
        throw(
            Kern_Exception(-ENOSYS, "syscall_set_attribute with given request is not implemented"));
        break;
    }
#else
    throw(Kern_Exception(-ENOSYS, "syscall_set_attribute is not implemented"));
#endif
}

void syscall_configure_system(u64 type, u64 arg1, u64 arg2, u64, u64, u64)
{
    task_ptr task = get_current_task();
    // TODO: Check permissions

    switch (type) {
        // case SYS_CONF_LAPIC:
        //      syscall_ret_high(get_current_task()) = lapic_configure(arg1, arg2);
        //     break;

        // case SYS_CONF_CPU:
        //     syscall_ret_high(get_current_task()) = cpu_configure(arg1, arg2);
        //     break;

        // case SYS_CONF_SLEEP10:
        //     pit_sleep_100us(arg1);
        //     break;

    default:
        throw(Kern_Exception(-ENOSYS, "syscall_configure_system with unknown parameter"));
        break;
    };
}

void syscall_set_priority(u64 priority, u64, u64, u64, u64, u64)
{
    task_ptr current_task = get_current_task();

    // SUCCESS will be overriden on error
    syscall_ret_low(current_task) = SUCCESS;

    if (priority >= sched_queues_levels) {
        throw(Kern_Exception(-ENOTSUP, "priority outside of queue levels"));
    }

    {
        Auto_Lock_Scope lock(current_task->sched_lock);
        current_task->priority = priority;
    }

    reschedule();
}

void syscall_get_lapic_id(u64 cpu_id, u64, u64, u64, u64, u64)
{
#ifdef __x86_64__

    CPU_Info *i;
    if (cpu_id == 0) {
        i = get_cpu_struct();
    } else if (cpu_id > cpus.size())
        throw Kern_Exception(-EINVAL, "CPU ID is out of range");
    else
        i = cpus[cpu_id - 1];

    syscall_ret_low(get_current_task())  = SUCCESS;
    // TODO: Store lapic_id withouth shifting it
    syscall_ret_high(get_current_task()) = i->lapic_id << 24;

#else
    // Not available on RISC-V
    // TODO: This should be hart id
    // syscall_ret_high(get_current_task()) = get_cpu_struct()->lapic_id;
    throw Kern_Exception(-ENOSYS, "syscall_get_lapic_id is not implemented");
#endif
}

void syscall_set_task_name(u64 pid, u64 /* const char* */ string, u64 length, u64, u64, u64)
{
    const task_ptr &current = get_cpu_struct()->current_task;

    task_ptr t = get_task_throw(pid);

    auto str = klib::string::fill_from_user(reinterpret_cast<const char *>(string), length);

    if (not str.first) {
        return;
    }

    Auto_Lock_Scope scope_lock(t->name_lock);

    syscall_ret_low(current) = SUCCESS;
    t->name.swap(str.second);
}

void syscall_create_port(u64 owner, u64, u64, u64, u64, u64)
{
    const task_ptr &task = get_current_task();

    // TODO: Check permissions

    TaskDescriptor *t;

    if (owner == 0 or task->task_id == owner) {
        t = task;
    } else {
        t = get_task_throw(owner);
    }

    syscall_ret_low(task) = SUCCESS;
    auto new_port         = Port::atomic_create_port(t);
    if (!new_port)
        throw Kern_Exception(-ENOMEM, "Could not create a port");

    syscall_ret_high(task) = new_port->portno;
}

void syscall_set_interrupt(uint64_t port, u64 intno, u64 flags, u64, u64, u64)
{
#if defined(__riscv) || defined(__x86_64__)
    auto c               = get_cpu_struct();
    const task_ptr &task = c->current_task;

    auto port_ptr = Port::atomic_get_port(port);
    if (!port_ptr)
        throw Kern_Exception(-ENOENT, "could not find the port");

    syscall_ret_low(task)  = SUCCESS;
    syscall_ret_high(task) = intno;
    c->int_handlers.add_handler(intno, port_ptr);
#else
    #error Unknown architecture
#endif
}

void syscall_complete_interrupt(u64 intno, u64, u64, u64, u64, u64)
{
#if defined(__riscv) || defined(__x86_64__)
    auto c               = get_cpu_struct();
    const task_ptr &task = c->current_task;

    try {
        c->int_handlers.ack_interrupt(intno, task->task_id);
    } catch (Kern_Exception &e) {
        serial_logger.printf("Error acking interrupt: %s, task %i intno %i CPU %i task CPU %i\n",
                             e.err_message, task->task_id, intno, c->cpu_id,
                             task->cpu_affinity - 1);
        throw e;
    }
    syscall_ret_low(task) = SUCCESS;
#else
    #error Unknown architecture
#endif
}

void syscall_name_port(u64 portnum, u64 /*const char* */ name, u64 length, u64 flags, u64, u64)
{
    const task_ptr task = get_current_task();

    auto str = klib::string::fill_from_user(reinterpret_cast<const char *>(name), length);
    if (not str.first) {
        return;
    }

    Port *port = Port::atomic_get_port(portnum);
    if (not port) {
        throw(Kern_Exception(-ENOENT, "Port not found"));
    }

    Auto_Lock_Scope scope_lock2(global_named_ports.lock);

    Named_Port_Desc *named_port = global_named_ports.storage.find(str.second);

    if (named_port) {
        auto parent_port = Port::atomic_get_port(named_port->parent_port_id);
        if (parent_port) {
            Auto_Lock_Scope lock(parent_port->lock);
            if (parent_port->alive)
                throw(Kern_Exception(-EEXIST, "Named port with a given name already exists"));
        }

        {
            Auto_Lock_Scope scope_lock(port->lock);
            if (!port->alive)
                throw(Kern_Exception(-ENOENT, "Port is not alive"));
            named_port->parent_port_id = port->portno;
        }

        syscall_ret_low(task) = SUCCESS;

        for (const auto &t: named_port->actions) {
            t->do_action(port, str.second);
        }

        named_port->actions.clear();
    } else {
        klib::unique_ptr<Named_Port_Desc> new_desc =
            new Named_Port_Desc(klib::move(str.second), port);

        {
            Auto_Lock_Scope scope_lock(port->lock);
            if (!port->alive)
                throw(Kern_Exception(-ENOENT, "Port is not alive"));

            global_named_ports.storage.insert(new_desc.release());
        }

        syscall_ret_low(task) = SUCCESS;
    }
}

void syscall_get_port_by_name(u64 /* const char * */ name, u64 length, u64 flags, u64, u64, u64)
{
    // --------------------- TODO: shared pointers *will* explode if the TaskDescriptor is deleted
    // -------------------------------------
    constexpr unsigned flag_noblock = 0x01;

    task_ptr task = get_current_task();

    auto str = klib::string::fill_from_user(reinterpret_cast<const char *>(name), length);
    if (not str.first) {
        return;
    }

    Auto_Lock_Scope scope_lock(global_named_ports.lock);
    auto named_port = global_named_ports.storage.find(str.second);

    if (named_port) {
        auto parent_port = Port::atomic_get_port(named_port->parent_port_id);
        if (parent_port) {
            Auto_Lock_Scope lock(parent_port->lock);
            if (!parent_port->alive) {
                throw(Kern_Exception(-ENOENT, "named port parent is expired"));
            } else {
                syscall_ret_low(task)  = SUCCESS;
                syscall_ret_high(task) = parent_port->portno;
            }
            return;
        } else {
            if (flags & flag_noblock) {
                throw(Kern_Exception(-ENOENT, "requested named port does not exist"));
                return;
            } else {
                named_port->actions.push_back(klib::make_unique<Notify_Task>(task, named_port));

                task->request_repeat_syscall();
                block_current_task(named_port);
            }
        }
    } else {
        if (flags & flag_noblock) {
            throw(Kern_Exception(-ENOENT, "requested named port does not exist"));
        } else {
            auto new_desc = klib::make_unique<Named_Port_Desc>(klib::move(str.second), nullptr);

            new_desc->actions.push_back(klib::make_unique<Notify_Task>(task, new_desc.get()));
            global_named_ports.storage.insert(new_desc.get());

            task->request_repeat_syscall();
            block_current_task(new_desc.release());
        }
    }
}

void syscall_request_named_port(u64 string_ptr, u64 length, u64 reply_port, u64 flags, u64, u64)
{
    const task_ptr task = get_current_task();

    auto str = klib::string::fill_from_user(reinterpret_cast<char *>(string_ptr), length);
    if (not str.first) {
        return;
    }

    // From this point on, the process can't block
    syscall_ret_low(task) = SUCCESS;

    Port *port_ptr = Port::atomic_get_port(reply_port);
    if (not port_ptr) {
        throw(Kern_Exception(-ENOENT, "port not found"));
    }

    Auto_Lock_Scope scope_lock(global_named_ports.lock);

    Named_Port_Desc *named_port = global_named_ports.storage.find(str.second);
    if (named_port) {
        auto parent_port = Port::atomic_get_port(named_port->parent_port_id);
        if (parent_port) {
            bool alive = false;
            {
                Auto_Lock_Scope lock(parent_port->lock);
                alive = parent_port->alive;
            }

            if (alive) {
                Send_Message msg(port_ptr);
                msg.do_action(parent_port, str.second);
                return;
            }
        }
        named_port->actions.push_back(klib::make_unique<Send_Message>(port_ptr));
    } else {
        auto new_desc =
            klib::make_unique<Named_Port_Desc>(Named_Port_Desc(klib::move(str.second), nullptr));

        new_desc->actions.push_back(klib::make_unique<Send_Message>(port_ptr));

        global_named_ports.storage.insert(new_desc.release());
    }
}

void syscall_set_log_port(u64 port, u64 flags, u64, u64, u64, u64)
{
    const task_ptr &task = get_current_task();

    Port *port_ptr = Port::atomic_get_port(port);
    if (not port_ptr)
        throw(Kern_Exception(-ENOENT, "port not found"));

    global_logger.set_port(port_ptr, flags);
    syscall_ret_low(task) = SUCCESS;
}

void syscall_create_normal_region(u64 pid, u64 addr_start, u64 size, u64 access, u64, u64)
{
    TaskDescriptor *current   = get_cpu_struct()->current_task;
    TaskDescriptor *dest_task = nullptr;

    if (pid == 0 or current->task_id == pid)
        dest_task = current;

    if (not dest_task)
        dest_task = get_task_throw(pid);

    // Syscall must be page aligned
    if (addr_start & 07777 or size & 07777) {
        throw(Kern_Exception(-EINVAL, "arguments are not page aligned"));
        return;
    }

    syscall_ret_low(current)  = SUCCESS;
    syscall_ret_high(current) = dest_task->page_table->atomic_create_normal_region(
        addr_start, size, access & 0x07, access & 0x08, access & 0x10, "anonymous region", 0);
}

void syscall_create_phys_map_region(u64 pid, u64 addr_start, u64 size, u64 access, u64 phys_addr,
                                    u64)
{
    TaskDescriptor *current   = get_cpu_struct()->current_task;
    TaskDescriptor *dest_task = nullptr;

    if (pid == 0 or current->task_id == pid)
        dest_task = current;

    if (not dest_task)
        dest_task = get_task_throw(pid);

    // Syscall must be page aligned
    if (addr_start & 07777 or size & 07777) {
        throw(Kern_Exception(-EINVAL, "arguments are not page aligned"));
        return;
    }

    syscall_ret_low(current)  = SUCCESS;
    syscall_ret_high(current) = dest_task->page_table->atomic_create_phys_region(
        addr_start, size, access & 0x07, access & 0x08, klib::string(), phys_addr);
}

void syscall_get_page_table(u64 pid, u64, u64, u64, u64, u64)
{
    TaskDescriptor *current  = get_cpu_struct()->current_task;
    syscall_ret_low(current) = SUCCESS;

    TaskDescriptor *target {};

    if (pid == 0 or current->task_id == pid)
        target = current;
    else
        target = get_task_throw(pid);

    Auto_Lock_Scope target_lock(target->sched_lock);

    if (not target->page_table) {
        throw(Kern_Exception(-ENOENT, "process has no page table"));
    }

    syscall_ret_high(current) = target->page_table->id;
}

// TODO: This should be renamed, as %fs and %gs segments are x86-64 specific thing, used to hold
// thread-local storage and stuff. Other architectures also store thread and global pointers
// somewhere, so this syscall is essentially that
void syscall_set_segment(u64 pid, u64 segment_type, u64 ptr, u64, u64, u64)
{
    TaskDescriptor *current = get_cpu_struct()->current_task;
    TaskDescriptor *target {};

    if (pid == 0 or current->task_id == pid)
        target = current;
    else
        target = get_task_throw(pid);

#ifdef __x86_64__
    if (target == current)
        save_segments(target);
#endif

    switch (segment_type) {
    case 1:
        // TODO: Make segments and registers consistent
        target->regs.thread_pointer() = ptr;
        break;
    case 2:
        target->regs.global_pointer() = ptr;
        break;
    case 3: {
        auto b = copy_from_user((char *)&target->regs, (char *)ptr, sizeof(target->regs));
        if (not b)
            return;
        break;
    }
    default:
        throw Kern_Exception(-ENOTSUP, "invalid segment in syscall_set_segment");
    }

#ifdef __x86_64__
    if (target == current)
        restore_segments(target);
#endif
    syscall_ret_low(current) = SUCCESS;
}

void syscall_get_segment(u64 pid, u64 segment_type, u64 ptr, u64, u64, u64)
{
    TaskDescriptor *current  = get_cpu_struct()->current_task;
    syscall_ret_low(current) = SUCCESS;

    TaskDescriptor *target {};
    u64 segment = 0;

    if (pid == 0 or current->task_id == pid)
        target = current;
    else
        target = get_task_throw(pid);

    switch (segment_type) {
    case 1: {
        segment = target->regs.thread_pointer();
        auto b =
            copy_to_user((char *)&current->regs.thread_pointer(), (char *)ptr, sizeof(segment));
        if (not b)
            return;
        break;
    }
    case 2: {
        segment = target->regs.global_pointer();
        auto b =
            copy_to_user((char *)&current->regs.global_pointer(), (char *)ptr, sizeof(segment));
        if (not b)
            return;
        break;
    }
    case 3: {
        // This is very bad (it overwrites user memory that is not supposed to be)...
        auto b = copy_to_user((char *)&target->regs, (char *)ptr, sizeof(target->regs));
        if (not b)
            return;
        break;
    }
    default:
        throw Kern_Exception(-ENOTSUP, "invalid segment in syscall_set_segment");
    }
}

void syscall_transfer_region(u64 to_page_table, u64 region, u64 dest, u64 flags, u64, u64)
{
    TaskDescriptor *current = get_current_task();

    klib::shared_ptr<Arch_Page_Table> pt = Arch_Page_Table::get_page_table_throw(to_page_table);

    bool fixed = flags & 0x08;

    const auto result = current->page_table->atomic_transfer_region(pt, region, dest, flags, fixed);

    syscall_ret_low(current)  = SUCCESS;
    syscall_ret_high(current) = result;
}

void syscall_asign_page_table(u64 pid, u64 page_table, u64 flags, u64, u64, u64)
{
    TaskDescriptor *current  = get_current_task();
    TaskDescriptor *dest     = get_task_throw(pid);
    syscall_ret_low(current) = SUCCESS;

    switch (flags) {
    case 1: // PAGE_TABLE_CREATE
        dest->create_new_page_table();
        syscall_ret_high(current) = dest->page_table->id;
        break;
    case 2: // PAGE_TABLE_ASIGN
    {
        klib::shared_ptr<Arch_Page_Table> t;
        if (page_table == 0 or page_table == current->page_table->id)
            t = current->page_table;
        else
            t = Arch_Page_Table::get_page_table_throw(page_table);

        dest->atomic_register_page_table(klib::forward<klib::shared_ptr<Arch_Page_Table>>(t));
        syscall_ret_high(current) = dest->page_table->id;
        break;
    }
    case 3: // PAGE_TABLE_CLONE
    {
        klib::shared_ptr<Arch_Page_Table> t = current->page_table->create_clone();
        dest->atomic_register_page_table(klib::forward<klib::shared_ptr<Arch_Page_Table>>(t));
        syscall_ret_high(current) = dest->page_table->id;
        break;
    }
    default:
        throw Kern_Exception(-ENOTSUP, "value of flags parameter is not supported");
    }
}

void syscall_create_mem_object(u64 size_bytes, u64, u64, u64, u64, u64)
{
    const auto &current_task       = get_current_task();
    const auto &current_page_table = current_task->page_table;
    syscall_ret_low(current_task)  = SUCCESS;

    // TODO: Remove hard-coded page sizes

    // Syscall must be page aligned
    if (size_bytes & 07777)
        throw Kern_Exception(-EINVAL, "arguments are not page aligned");

    const auto page_size_log = 12; // 2^12 = 4096 bytes
    const auto size_pages    = size_bytes / 4096;

    const auto ptr = Mem_Object::create(page_size_log, size_pages);

    current_page_table->atomic_pin_memory_object(ptr);

    syscall_ret_high(current_task) = ptr->get_id();
}

void syscall_map_mem_object(u64 page_table_id, u64 addr_start, u64 size_bytes, u64 access,
                            u64 object_id, u64 offset)
{
    const auto &current_task = get_current_task();
    auto table               = page_table_id == 0 ? current_task->page_table
                                                  : Arch_Page_Table::get_page_table_throw(page_table_id);

    if ((size_bytes == 0) or ((size_bytes & 0xfff) != 0))
        throw Kern_Exception(-EINVAL, "size not page aligned");

    if (offset & 0xfff)
        throw Kern_Exception(-EINVAL, "offset not page aligned");

    const auto object = Mem_Object::get_object(object_id);

    if (object->size_bytes() < offset + size_bytes)
        throw Kern_Exception(-EFBIG, "size is out of range");

    syscall_ret_low(current_task)  = SUCCESS;
    auto res = table->atomic_create_mem_object_region(
        addr_start, size_bytes, access & 0x7, access & 0x8, "object map", object, access & 0x10, 0,
        offset, size_bytes);
    
    if (!res.success())
        throw Kern_Exception(res.result, "error in atomic_create_mem_object_region");
    syscall_ret_high(current_task) = res.val->start_addr;
}

void syscall_delete_region(u64 tid, u64 region_start, u64, u64, u64, u64)
{
    syscall_ret_low(get_current_task()) = SUCCESS;

    // 0 is TASK_ID_SELF
    const auto task        = tid == 0 ? get_cpu_struct()->current_task : get_task_throw(tid);
    const auto &page_table = task->page_table;

    // TODO: Contemplate about adding a check to see if it's inside the kernels address space

    // TODO: This is completely untested and knowing me is probably utterly broken
    page_table->atomic_delete_region(region_start);
}

void syscall_unmap_range(u64 task_id, u64 addr_start, u64 size, u64, u64, u64)
{
    syscall_ret_low(get_current_task()) = SUCCESS;

    const auto task = task_id == 0 ? get_cpu_struct()->current_task : get_task_throw(task_id);
    const auto &page_table = task->page_table;

    // Maybe EINVAL if unaligned?
    auto offset             = addr_start & (PAGE_SIZE - 1);
    auto addr_start_aligned = addr_start - offset;
    auto size_aligned       = (size + offset + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    page_table->atomic_release_in_range(addr_start_aligned, size_aligned);
}

void syscall_remove_from_task_group(u64 pid, u64 group, u64, u64, u64, u64)
{
    syscall_ret_low(get_current_task()) = SUCCESS;

    const auto task = pid == 0 ? get_current_task() : get_task_throw(pid);

    const auto group_ptr = TaskGroup::get_task_group_throw(group);

    group_ptr->atomic_remove_task(task);
}

void syscall_create_task_group(u64, u64, u64, u64, u64, u64)
{
    const auto &current_task  = get_current_task();
    const auto new_task_group = TaskGroup::create();

    syscall_ret_low(current_task) = SUCCESS;

    new_task_group->atomic_register_task(current_task);

    syscall_ret_high(current_task) = new_task_group->get_id();
}

void syscall_is_in_task_group(u64 pid, u64 group, u64, u64, u64, u64)
{
    syscall_ret_low(get_current_task()) = SUCCESS;

    const u64 current_pid = pid == 0 ? get_current_task()->task_id : pid;
    const auto group_ptr  = TaskGroup::get_task_group_throw(group);

    const bool has_task                  = group_ptr->atomic_has_task(current_pid);
    syscall_ret_high(get_current_task()) = has_task;
}

void syscall_add_to_task_group(u64 pid, u64 group, u64, u64, u64, u64)
{
    syscall_ret_low(get_current_task()) = SUCCESS;

    const auto task = pid == 0 ? get_current_task() : get_task_throw(pid);

    const auto group_ptr = TaskGroup::get_task_group_throw(group);

    group_ptr->atomic_register_task(task);
}

void syscall_set_notify_mask(u64 task_group, u64 port_id, u64 new_mask, u64 flags, u64, u64)
{
    const auto group = TaskGroup::get_task_group_throw(task_group);
    const auto port  = Port::atomic_get_port(port_id);
    if (!port)
        throw Kern_Exception(-ENOENT, "port not found");

    u64 old_mask = group->atomic_change_notifier_mask(port, new_mask, flags);

    syscall_ret_low(get_current_task())  = SUCCESS;
    syscall_ret_high(get_current_task()) = old_mask;
}

void syscall_request_timer(u64 port, u64 timeout, u64, u64, u64, u64)
{
    const auto port_ptr = Port::atomic_get_port(port);
    if (!port_ptr)
        throw Kern_Exception(-ENOENT, "port not found");
    auto c                           = get_cpu_struct();
    syscall_ret_low(c->current_task) = SUCCESS;

    u64 core_time_ms = c->ticks_after_ns(timeout);
    c->atomic_timer_queue_push(core_time_ms, port_ptr);
}

void syscall_set_affinity(u64 pid, u64 affinity, u64 flags, u64, u64, u64)
{
    const auto current_cpu = get_cpu_struct();
    const auto task        = pid == 0 ? current_cpu->current_task : get_task_throw(pid);
    const auto cpu         = affinity == -1UL ? current_cpu->cpu_id + 1 : affinity;

    const auto cpu_count = get_cpu_count();
    if (cpu > cpu_count) {
        throw Kern_Exception(-ENOENT, "cpu id is out of range");
    }

    if (task != current_cpu->current_task) {
        Auto_Lock_Scope lock(task->sched_lock);
        if (task->status != TaskStatus::TASK_PAUSED)
            throw Kern_Exception(-EBUSY, "task is not paused");

        if (not task->can_be_rebound())
            throw Kern_Exception(-EPERM, "task can't be rebound");

        syscall_ret_low(current_cpu->current_task) = SUCCESS;
        syscall_ret_high(task)                     = task->cpu_affinity;
        task->cpu_affinity                         = cpu;
    } else {
        if (not task->can_be_rebound())
            throw Kern_Exception(-EPERM, "task can't be rebound");

        if (cpu != 0 && cpu != (current_cpu->cpu_id + 1)) {
            syscall_ret_low(task) = SUCCESS;
            find_new_process();

            {
                Auto_Lock_Scope lock(task->sched_lock);
                syscall_ret_high(task) = task->cpu_affinity;
                task->cpu_affinity     = cpu;
                push_ready(task);
            }

            auto remote_cpu = cpus[cpu - 1];
            assert(remote_cpu->cpu_id == cpu - 1);
            if (remote_cpu->current_task_priority > task->priority)
                remote_cpu->ipi_reschedule();
        } else {
            syscall_ret_low(task) = SUCCESS;
            Auto_Lock_Scope lock(task->sched_lock);
            syscall_ret_high(task) = task->cpu_affinity;
            task->cpu_affinity     = cpu;
        }
    }

    // serial_logger.printf("Task %d (%s) affinity set to %d\n", task->task_id, task->name.c_str(),
    // cpu);

    reschedule();
}

void syscall_yield(u64, u64, u64, u64, u64, u64)
{
    syscall_ret_low(get_current_task()) = SUCCESS;
    reschedule();
}

void syscall_get_time(u64 mode, u64, u64, u64, u64, u64)
{
    const auto current_task = get_current_task();

    switch (mode) {
    case GET_TIME_NANOSECONDS_SINCE_BOOTUP:
        syscall_ret_low(current_task)  = SUCCESS;
        syscall_ret_high(current_task) = get_ns_since_bootup();
        break;
    case GET_TIME_REALTIME_NANOSECONDS:
        syscall_ret_low(current_task)  = SUCCESS;
        syscall_ret_high(current_task) = unix_time_bootup * 1000000000 + get_ns_since_bootup();
        break;
    default:
        throw Kern_Exception(-ENOTSUP, "unknown mode in syscall_get_time");
    }
}

void syscall_system_info(u64 param, u64, u64, u64, u64, u64)
{
    const auto current_task = get_current_task();

    switch (param) {
    case SYSINFO_NPROCS:
    case SYSINFO_NPROCS_CONF:
        syscall_ret_low(current_task)  = SUCCESS;
        syscall_ret_high(current_task) = get_cpu_count();
        break;
    default:
        throw Kern_Exception(-ENOTSUP, "unknown param in syscall_system_info");
    }
}

void syscall_pause_task(u64 task_id, u64, u64, u64, u64, u64)
{
    const auto current_task = get_current_task();
    if (current_task->regs.syscall_pending_restart())
        current_task->pop_repeat_syscall();

    const auto task = task_id == 0 ? current_task : get_task_throw(task_id);
    if (task->is_kernel_task())
        throw Kern_Exception(-EPERM, "can't pause system task");

    Auto_Lock_Scope lock(task->sched_lock);
    switch (task->status) {
    case TaskStatus::TASK_RUNNING: {
        if (current_task == task) {
            task->status = TaskStatus::TASK_PAUSED;

            syscall_ret_low(current_task) = SUCCESS;

            task->parent_queue = &paused;
            {
                Auto_Lock_Scope lock(paused.lock);
                paused.push_back(task);
            }
            find_new_process();
        } else {
            {
                Auto_Lock_Scope lock(current_task->sched_lock);
                if (current_task->status == TaskStatus::TASK_DYING)
                    throw Kern_Exception(-EINVAL, "can't block dying task");

                current_task->request_repeat_syscall();

                current_task->status       = TaskStatus::TASK_BLOCKED;
                current_task->blocked_by   = nullptr;
                current_task->parent_queue = &task->waiting_to_pause;
                {
                    Auto_Lock_Scope lock(task->waiting_to_pause.lock);
                    task->waiting_to_pause.push_back(current_task);
                }
                find_new_process();
            }

            __atomic_or_fetch(&task->sched_pending_mask, TaskDescriptor::SCHED_PENDING_PAUSE,
                              __ATOMIC_RELEASE);
            // TODO: IPI other CPU
        }
        break;
    }
    case TaskStatus::TASK_PAUSED:
        syscall_ret_low(current_task) = SUCCESS;
        return;
    case TaskStatus::TASK_READY: {
        task->status = TaskStatus::TASK_PAUSED;
        task->parent_queue->atomic_erase(task);

        syscall_ret_low(current_task) = SUCCESS;

        task->parent_queue = &paused;
        Auto_Lock_Scope lock(paused.lock);
        paused.push_back(task);
        break;
    }
    case TaskStatus::TASK_UNINIT:
        throw Kern_Exception(-EINVAL, "process is not in UNINIT state");
    case TaskStatus::TASK_SPECIAL:
        throw Kern_Exception(-EPERM, "can't pause special task");
    case TaskStatus::TASK_DYING:
    case TaskStatus::TASK_DEAD:
        throw Kern_Exception(-EINVAL, "can't pause dead task");
    case TaskStatus::TASK_BLOCKED: {
        if (task->regs.syscall_pending_restart())
            task->interrupt_restart_syscall();

        task->status = TaskStatus::TASK_PAUSED;
        task->parent_queue->atomic_erase(task);

        syscall_ret_low(current_task) = SUCCESS;

        task->parent_queue = &paused;
        Auto_Lock_Scope lock(paused.lock);
        paused.push_back(task);
        break;
    }
    }
}

void syscall_resume_task(u64 task_id, u64, u64, u64, u64, u64)
{
    // TODO: Start task syscall is redundant
    const auto current_task = get_current_task();
    const auto task         = get_task_throw(task_id);

    Auto_Lock_Scope lock(task->sched_lock);
    syscall_ret_low(current_task) = SUCCESS;

    if (task->is_uninited() or task->status == TaskStatus::TASK_PAUSED)
        task->init();
    else if (task->status == TaskStatus::TASK_RUNNING)
        return;
    else
        throw Kern_Exception(-EINVAL, "can't resume task");
}

void syscall_get_page_address(u64 task_id, u64 page_base, u64 flags, u64, u64, u64)
{
    if (page_base & (PAGE_SIZE - 1))
        throw Kern_Exception(-EINVAL, "page_base is not page aligned");

    auto task = task_id == 0 ? get_current_task() : get_task_throw(task_id);

    if (task->status == TaskStatus::TASK_UNINIT)
        throw Kern_Exception(-EINVAL, "task is not initialized");

    syscall_ret_low(task) = SUCCESS;

    auto table = task->page_table;
    if (not table)
        throw Kern_Exception(-ENOENT, "task has no page table");

    Auto_Lock_Scope lock(table->lock);

    auto b = table->prepare_user_page(page_base, 0);
    if (not b)
        return;

    auto mapping           = table->get_page_mapping(page_base);
    syscall_ret_high(task) = mapping.page_addr;
}