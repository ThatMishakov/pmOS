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

using syscall_function                          = void (*)();
klib::array<syscall_function, 49> syscall_table = {
    syscall_exit,
    syscall_get_task_id,
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

    unsigned call_n = syscall_number(task);

    // The syscall number is sometimes overwritten which causes issues
    // This is a "temporary" workaround
    if (task->regs.syscall_pending_restart())
        call_n = task->syscall_num;
    task->syscall_num = call_n;

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

    // TODO: This crashes if the syscall is not implemented
    if (call_n >= syscall_table.size() or syscall_table[call_n] == nullptr) {
        t_print_bochs("Debug: syscall %h pid %h (%s) ", call_n, task->task_id, task->name.c_str());
        t_print_bochs(" -> %i (%s)\n", -ENOTSUP, "syscall not implemented");
        syscall_error(task) = -ENOTSUP;
        return;
    }

    syscall_table[call_n]();

    if ((syscall_error(task) < 0) && !task->regs.syscall_pending_restart()) {
        t_print_bochs("Debug: syscall %h (%i) pid %h (%s) ", call_n, call_n, task->task_id,
                      task->name.c_str());
        i64 val = syscall_error(task);
        t_print_bochs(" -> %i (%s)\n", val, "syscall failed");
    }

    // t_print_bochs("SUCCESS %h\n", syscall_ret_high(task));

    // t_print_bochs(" -> SUCCESS\n");
}

void syscall_get_task_id()
{
    const task_ptr &task = get_cpu_struct()->current_task;

    syscall_return(task) = task->task_id;
}

void syscall_create_process()
{
    auto task   = get_current_task();
    auto result = TaskDescriptor::create_process(TaskDescriptor::PrivilegeLevel::User);
    if (result) {
        syscall_return(task) = result->task_id;
    } else {
        syscall_error(task) = -ENOMEM;
    }
}

void syscall_start_process()
{
    task_ptr task = get_current_task();
    // TODO: Check permissions

    u64 pid     = syscall_arg64(task, 0);
    ulong start = syscall_arg(task, 1, 1);
    ulong arg1  = syscall_arg(task, 2, 1);
    ulong args[2];
    auto result = syscall_args_checked(task, 3, 1, 2, args);
    if (!result.success()) {
        syscall_error(task) = result.result;
        return;
    }
    if (!result.val) {
        return;
    }
    ulong arg2 = args[0];
    ulong arg3 = args[1];

    TaskDescriptor *t = get_task(pid);
    if (!t) {
        syscall_error(task) = -ESRCH;
        return;
    }

    Auto_Lock_Scope scope_sched_lock(t->sched_lock);

    // If the process is running, you can't start it again
    if (not t->is_uninited()) {
        syscall_error(task) = -EBUSY;
        return;
    }

    // Set entry
    t->set_entry_point(start);

    // Pass arguments
    t->regs.arg1() = arg1;
    t->regs.arg2() = arg2;
    t->regs.arg3() = arg3;

    syscall_return(task) = 0;

    // Init task
    t->init();
}

void syscall_load_executable()
{
    task_ptr task = get_current_task();
    // TODO: Check permissions

    u64 task_id   = syscall_arg64(task, 0);
    u64 object_id = syscall_arg64(task, 1);
    ulong flags   = syscall_flags(task);

    TaskDescriptor *t = get_task(task_id);
    if (!t) {
        syscall_error(task) = -ESRCH;
        return;
    }

    klib::shared_ptr<Mem_Object> object = Mem_Object::get_object(object_id);
    if (!object) {
        syscall_error(task) = -ENOENT;
        return;
    }

    klib::string name;
    {
        Auto_Lock_Scope scope_lock(t->name_lock);
        name = t->name.clone();
        if (name.empty() && !t->name.empty()) {
            syscall_error(task) = -ENOMEM;
            return;
        }
    }

    auto b = t->load_elf(object, name);
    if (!b.success()) {
        syscall_error(task) = b.result;
        return;
    }

    // Blocking is not implemented. Return error
    if (!b.val) {
        syscall_error(task) = -ENOSYS;
        return;
    }

    // TODO: This will race once blocking is to be implemented
    syscall_success(task);
}

void syscall_init_stack()
{
    const task_ptr &task = get_current_task();

    u64 pid   = syscall_arg64(task, 0);
    ulong esp = syscall_arg(task, 1, 1);

    // TODO: Check permissions

    TaskDescriptor *t = get_task(pid);
    if (!t) {
        syscall_error(task) = -ESRCH;
        return;
    }

    Auto_Lock_Scope scope_sched_lock(t->sched_lock);

    // If the process is running, you can't start it again
    if (not t->is_uninited()) {
        syscall_error(task) = -EBUSY;
        return;
    }

    if (esp == 0) { // If ESP == 0 use default kernel's stack policy
        auto result = t->init_stack();
        if (result.success()) {
            syscall_return(task) = result.val;
        } else {
            syscall_error(task) = result.result;
        }
    } else {
        t->regs.stack_pointer() = esp;
        syscall_return(task)    = esp;
    }
}

void syscall_exit()
{
    TaskDescriptor *task = get_cpu_struct()->current_task;

    ulong arg1 = syscall_arg(task, 0, 0);
    ulong arg2 = syscall_arg(task, 1, 0);

    // Record exit code
    task->ret_hi = arg2;
    task->ret_lo = arg1;

    syscall_success(task);
    // Kill the process
    task->atomic_kill();
}

void syscall_kill_task()
{
    TaskDescriptor *task = get_current_task();

    u64 task_id = syscall_arg64(task, 0);

    TaskDescriptor *t = get_task(task_id);
    if (!t) {
        syscall_error(task) = -ESRCH;
        return;
    }

    syscall_success(task);
    t->atomic_kill();
}

void syscall_get_first_message()
{
    TaskDescriptor *current = get_cpu_struct()->current_task;

    u64 portno = syscall_arg64(current, 0);
    ulong buff = syscall_arg(current, 1, 1);
    ulong args = syscall_flags(current);

    auto port = Port::atomic_get_port(portno);
    if (!port) {
        syscall_error(current) = -ENOENT;
        return;
    }

    klib::shared_ptr<Message> top_message;

    {
        Auto_Lock_Scope scope_lock(port->lock);

        if (current != port->owner) {
            syscall_error(current) = -EPERM;
            return;
        }

        if (port->is_empty()) {
            syscall_error(current) = -EAGAIN;
            return;
        }

        top_message = port->get_front();

        syscall_success(current);
        auto result = top_message->copy_to_user_buff((char *)buff);
        if (!result.success()) {
            syscall_error(current) = result.result;
            return;
        }

        if (not result.val)
            return;

        if (!(args & MSG_ARG_NOPOP)) {
            port->pop_front();
        }
    }
}

void syscall_send_message_port()
{
    TaskDescriptor *current = get_cpu_struct()->current_task;

    u64 port_num  = syscall_arg64(current, 0);
    size_t size   = syscall_arg(current, 1, 1);
    ulong message = syscall_arg(current, 2, 1);

    // TODO: Check permissions

    auto port = Port::atomic_get_port(port_num);
    if (!port) {
        syscall_error(current) = -ENOENT;
        return;
    }

    syscall_success(current);
    auto result = port->atomic_send_from_user(current, (char *)message, size);
    if (!result.success()) {
        syscall_error(current) = result.result;
        return;
    }

    if (!result.val)
        return;

    // TODO: This is problematic if the task switches
}

void syscall_get_message_info()
{
    task_ptr task = get_current_task();

    u64 portno           = syscall_arg64(task, 0);
    ulong message_struct = syscall_arg(task, 1, 1);
    ulong flags          = syscall_flags(task);

    auto port = Port::atomic_get_port(portno);
    if (!port) {
        syscall_error(task) = -ENOENT;
        return;
    }

    if (port->owner != task) {
        syscall_error(task) = -EPERM;
        return;
    }

    klib::shared_ptr<Message> msg;

    {
        Auto_Lock_Scope lock(port->lock);
        if (port->is_empty()) {
            constexpr unsigned FLAG_NOBLOCK = 0x01;

            if (flags & FLAG_NOBLOCK) {
                syscall_error(task) = -EAGAIN;
                return;
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

    syscall_success(task);
    auto b = copy_to_user((char *)&desc, (char *)message_struct, msg_struct_size);
    if (!b.success()) {
        syscall_error(task) = b.result;
        return;
    }

    if (not b.val)
        assert(!"blocking by page is not yet implemented");
}

void syscall_set_attribute()
{
    task_ptr task = get_current_task();

    u64 pid         = syscall_arg64(task, 0);
    ulong attribute = syscall_arg(task, 1, 1);
    ulong value     = syscall_arg(task, 2, 1);

    // TODO: Check persmissions

    // TODO: This is *very* x86-specific and is a bad idea in general

#if defined(__x86_64__) || defined(__i386__)
    TaskDescriptor *process = pid == task->task_id ? task : get_task(pid);
    if (!process) {
        syscall_error(task) = -ESRCH;
        return;
    }

    Interrupt_Stackframe *current_frame = &process->regs.e;

    switch (attribute) {
    case ATTR_ALLOW_PORT:
        current_frame->rflags.bits.iopl = value ? 3 : 0;
        break;

    case ATTR_DEBUG_SYSCALLS:
        process->attr.debug_syscalls = value;
        break;

    default:
        syscall_error(task) = -ENOTSUP;
        break;
    }
#else
    syscall_error(task) = -ENOSYS;
#endif
}

void syscall_configure_system() // u64 type, u64 arg1, u64 arg2, u64, u64, u64
{
    task_ptr task = get_current_task();
    ulong type    = syscall_arg(task, 0, 0);
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
        syscall_error(task) = -ENOSYS;
        break;
    };
}

void syscall_set_priority()
{
    task_ptr current_task = get_current_task();

    ulong priority = syscall_arg(current_task, 0, 0);

    // SUCCESS will be overriden on error
    syscall_success(current_task);

    if (priority >= sched_queues_levels) {
        syscall_error(current_task) = -ENOTSUP;
        return;
    }

    {
        Auto_Lock_Scope lock(current_task->sched_lock);
        current_task->priority = priority;
    }

    reschedule();
}

void syscall_get_lapic_id()
{
    auto current_task = get_current_task();
    ulong cpu_id      = syscall_arg(current_task, 0, 0);
#ifdef __x86_64__

    CPU_Info *i;
    if (cpu_id == 0) {
        i = get_cpu_struct();
    } else if (cpu_id > cpus.size()) {
        syscall_error(current_task) = -ENOENT;
        return;
    } else
        i = cpus[cpu_id - 1];

    // TODO: Store lapic_id withouth shifting it
    syscall_return(current_task) = i->lapic_id << 24;

#else
    // Not available on RISC-V
    // TODO: This should be hart id
    // syscall_ret_high(get_current_task()) = get_cpu_struct()->lapic_id;
    syscall_error(current_task) = -ENOSYS;
#endif
}

void syscall_set_task_name()
{
    task_ptr current = get_cpu_struct()->current_task;

    u64 pid      = syscall_arg64(current, 0);
    ulong string = syscall_arg(current, 1, 1);
    ulong length = syscall_arg(current, 2, 1);

    task_ptr t = get_task(pid);
    if (!t) {
        syscall_error(current) = -ESRCH;
        return;
    }

    syscall_success(current);

    klib::string name(length, 0);
    auto b = copy_from_user((char *)name.data(), (char *)string, length);
    if (!b.success()) {
        syscall_error(current) = b.result;
        return;
    }

    if (!b.val)
        return;

    Auto_Lock_Scope scope_lock(t->name_lock);
    t->name.swap(name);
}

void syscall_create_port()
{
    const task_ptr &task = get_current_task();

    u64 owner = syscall_arg64(task, 0);

    // TODO: Check permissions

    TaskDescriptor *t;

    if (owner == 0 or task->task_id == owner) {
        t = task;
    } else {
        t = get_task(owner);
    }
    if (!t) {
        syscall_error(task) = -ESRCH;
        return;
    }

    auto new_port = Port::atomic_create_port(t);
    if (!new_port) {
        syscall_error(task) = -ENOMEM;
        return;
    }

    syscall_return(task) = new_port->portno;
}

void syscall_set_interrupt()
{
#if defined(__riscv) || defined(__x86_64__)
    auto c               = get_cpu_struct();
    const task_ptr &task = c->current_task;

    u64 port    = syscall_arg64(task, 0);
    ulong intno = syscall_arg(task, 1, 1);
    ulong flags = syscall_flags(task);

    auto port_ptr = Port::atomic_get_port(port);
    if (!port_ptr) {
        syscall_error(task) = -ENOENT;
        return;
    }

    syscall_return(task) = intno;
    auto result          = c->int_handlers.add_handler(intno, port_ptr);
    if (result < 0)
        syscall_error(task) = result;
#else
    #error Unknown architecture
#endif
}

void syscall_complete_interrupt()
{
#if defined(__riscv) || defined(__x86_64__)
    auto c               = get_cpu_struct();
    const task_ptr &task = c->current_task;

    ulong intno = syscall_arg(task, 0, 0);

    auto result = c->int_handlers.ack_interrupt(intno, task->task_id);
    if (result < 0) {
        serial_logger.printf("Error acking interrupt: %i, task %i intno %i CPU %i task CPU %i\n",
                             result, task->task_id, intno, c->cpu_id, task->cpu_affinity - 1);
    }
    syscall_error(task) = result;
#else
    #error Unknown architecture
#endif
}

void syscall_name_port()
{
    const task_ptr task = get_current_task();

    u64 portnum  = syscall_arg64(task, 0);
    ulong name   = syscall_arg(task, 1, 1);
    ulong length = syscall_arg(task, 2, 1);
    ulong flags  = syscall_flags(task);

    klib::string str(length, 0);
    auto b = copy_from_user((char *)str.data(), (char *)name, length);
    if (!b.success()) {
        syscall_error(task) = b.result;
        return;
    }

    if (!b.val)
        return;

    Port *port = Port::atomic_get_port(portnum);
    if (not port) {
        syscall_error(task) = -ENOENT;
        return;
    }

    Auto_Lock_Scope scope_lock2(global_named_ports.lock);

    Named_Port_Desc *named_port = global_named_ports.storage.find(str);

    if (named_port) {
        auto parent_port = Port::atomic_get_port(named_port->parent_port_id);
        if (parent_port) {
            Auto_Lock_Scope lock(parent_port->lock);
            if (parent_port->alive) {
                syscall_error(task) = -EEXIST;
                return;
            }
        }

        {
            Auto_Lock_Scope scope_lock(port->lock);
            if (!port->alive) {
                syscall_error(task) = -ENOENT;
                return;
            }
            named_port->parent_port_id = port->portno;
        }

        syscall_success(task);

        for (const auto &t: named_port->actions) {
            t->do_action(port, str);
        }

        named_port->actions.clear();
    } else {
        klib::unique_ptr<Named_Port_Desc> new_desc = new Named_Port_Desc(klib::move(str), port);
        if (!new_desc) {
            syscall_error(task) = -ENOMEM;
            return;
        }

        {
            Auto_Lock_Scope scope_lock(port->lock);
            if (!port->alive) {
                syscall_error(task) = -ENOENT;
                return;
            }

            global_named_ports.storage.insert(new_desc.release());
        }

        syscall_success(task);
    }
}

void syscall_get_port_by_name()
{
    // --------------------- TODO: shared pointers *will* explode if the TaskDescriptor is deleted
    // -------------------------------------
    constexpr unsigned flag_noblock = 0x01;

    task_ptr task = get_current_task();

    ulong name   = syscall_arg(task, 0, 0);
    ulong length = syscall_arg(task, 1, 0);
    ulong flags  = syscall_flags(task);

    klib::string str(length, 0);
    auto b = copy_from_user((char *)str.data(), (char *)name, length);
    if (!b.success()) {
        syscall_error(task) = b.result;
        return;
    }

    if (!b.val)
        return;

    Auto_Lock_Scope scope_lock(global_named_ports.lock);
    auto named_port = global_named_ports.storage.find(str);

    if (named_port) {
        auto parent_port = Port::atomic_get_port(named_port->parent_port_id);
        if (parent_port) {
            Auto_Lock_Scope lock(parent_port->lock);
            if (!parent_port->alive) {
                syscall_error(task) = -ENOENT;
            } else {
                syscall_return(task) = parent_port->portno;
            }
            return;
        } else {
            if (flags & flag_noblock) {
                syscall_error(task) = -ENOENT;
                return;
            } else {
                auto ptr = klib::make_unique<Notify_Task>(task, named_port);
                if (!ptr) {
                    syscall_error(task) = -ENOMEM;
                    return;
                }
                if (!named_port->actions.push_back(klib::move(ptr))) {
                    syscall_error(task) = -ENOMEM;
                    return;
                }

                task->request_repeat_syscall();
                block_current_task(named_port);
            }
        }
    } else {
        if (flags & flag_noblock) {
            syscall_error(task) = -ENOENT;
            return;
        } else {
            auto new_desc = klib::make_unique<Named_Port_Desc>(klib::move(str), nullptr);
            if (!new_desc) {
                syscall_error(task) = -ENOMEM;
                return;
            }

            if (!new_desc->actions.push_back(
                    klib::make_unique<Notify_Task>(task, new_desc.get()))) {
                syscall_error(task) = -ENOMEM;
                return;
            }

            global_named_ports.storage.insert(new_desc.get());

            task->request_repeat_syscall();
            block_current_task(new_desc.release());
        }
    }
}

void syscall_request_named_port()
{
    const task_ptr task = get_current_task();
    u64 reply_port      = syscall_arg64(task, 0);
    ulong string_ptr    = syscall_arg(task, 1, 1);
    ulong length        = syscall_arg(task, 2, 1);
    ulong flags         = syscall_flags(task);

    klib::string str(length, 0);
    auto b = copy_from_user((char *)str.data(), (char *)string_ptr, length);
    if (!b.success()) {
        syscall_error(task) = b.result;
        return;
    }

    if (!b.val)
        return;

    // From this point on, the process can't block
    syscall_success(task);

    Port *port_ptr = Port::atomic_get_port(reply_port);
    if (not port_ptr) {
        syscall_error(task) = -ENOENT;
        return;
    }

    Auto_Lock_Scope scope_lock(global_named_ports.lock);

    Named_Port_Desc *named_port = global_named_ports.storage.find(str);
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
                msg.do_action(parent_port, str);
                return;
            }
        }
        if (!named_port->actions.push_back(klib::make_unique<Send_Message>(port_ptr))) {
            syscall_error(task) = -ENOMEM;
            return;
        }
    } else {
        auto new_desc =
            klib::make_unique<Named_Port_Desc>(Named_Port_Desc(klib::move(str), nullptr));
        if (!new_desc) {
            syscall_error(task) = -ENOMEM;
            return;
        }

        if (!new_desc->actions.push_back(klib::make_unique<Send_Message>(port_ptr))) {
            syscall_error(task) = -ENOMEM;
            return;
        }

        global_named_ports.storage.insert(new_desc.release());
    }
}

void syscall_set_log_port()
{
    const task_ptr &task = get_current_task();

    u64 port    = syscall_arg64(task, 0);
    ulong flags = syscall_flags(task);

    Port *port_ptr = Port::atomic_get_port(port);
    if (not port_ptr) {
        syscall_error(task) = -ENOENT;
        return;
    }

    // TODO: Error checking?
    global_logger.set_port(port_ptr, flags);
    syscall_success(task);
}

void syscall_create_normal_region()
{
    TaskDescriptor *current = get_cpu_struct()->current_task;

    u64 pid          = syscall_arg64(current, 0);
    ulong addr_start = syscall_arg(current, 1, 1);
    ulong size       = syscall_arg(current, 2, 1);
    ulong access     = syscall_flags(current);

    TaskDescriptor *dest_task = nullptr;

    if (pid == 0 or current->task_id == pid)
        dest_task = current;

    if (not dest_task)
        dest_task = get_task(pid);

    if (not dest_task) {
        syscall_error(current) = -ESRCH;
        return;
    }

    // Syscall must be page aligned
    if (addr_start & 07777 or size & 07777) {
        syscall_error(current) = -EINVAL;
        return;
    }

    klib::string region_name("anonymous region");

    auto result = dest_task->page_table->atomic_create_normal_region(
        addr_start, size, access & 0x07, access & 0x08, access & 0x10, klib::move(region_name), 0,
        access & 0x20);
    if (!result.success()) {
        syscall_error(current) = result.result;
    } else {
        syscall_return(current) = result.val->start_addr;
    }
}

void syscall_create_phys_map_region()
{
    TaskDescriptor *current = get_cpu_struct()->current_task;
    u64 pid                 = syscall_arg64(current, 0);
    u64 phys_addr           = syscall_arg64(current, 1);

    ulong args[2];
    {
        auto result = syscall_args_checked(current, 2, 2, 2, args);
        if (!result.success()) {
            syscall_error(current) = result.result;
            return;
        }
        if (!result.val) {
            return;
        }
    }
    ulong addr_start = args[0];
    ulong size       = args[1];
    ulong access     = syscall_flags(current);

    TaskDescriptor *dest_task = nullptr;

    if (pid == 0 or current->task_id == pid)
        dest_task = current;

    if (not dest_task)
        dest_task = get_task(pid);

    if (not dest_task) {
        syscall_error(current) = -ESRCH;
        return;
    }

    // Syscall must be page aligned
    if (addr_start & 07777 or size & 07777) {
        syscall_error(current) = -EINVAL;
        return;
    }

    auto result = dest_task->page_table->atomic_create_phys_region(
        addr_start, size, access & 0x07, access & 0x08, klib::string(), phys_addr);
    if (!result.success()) {
        syscall_error(current) = result.result;
    } else {
        syscall_return(current) = result.val->start_addr;
    }
}

void syscall_get_page_table()
{
    TaskDescriptor *current = get_cpu_struct()->current_task;

    u64 pid = syscall_arg64(current, 0);

    syscall_success(current);

    TaskDescriptor *target {};

    if (pid == 0 or current->task_id == pid)
        target = current;
    else
        target = get_task(pid);

    if (not target) {
        syscall_error(current) = -ESRCH;
        return;
    }

    Auto_Lock_Scope target_lock(target->sched_lock);

    if (not target->page_table) {
        syscall_error(current) = -ENOENT;
        return;
    }

    syscall_return(current) = target->page_table->id;
}

// TODO: This should be renamed, as %fs and %gs segments are x86-64 specific thing, used to hold
// thread-local storage and stuff. Other architectures also store thread and global pointers
// somewhere, so this syscall is essentially that
void syscall_set_segment()
{
    TaskDescriptor *current = get_cpu_struct()->current_task;
    TaskDescriptor *target {};

    u64 pid            = syscall_arg64(current, 0);
    ulong segment_type = syscall_arg(current, 1, 1);
    ulong ptr          = syscall_arg(current, 2, 1);

    if (pid == 0 or current->task_id == pid)
        target = current;
    else
        target = get_task(pid);

    if (not target) {
        syscall_error(current) = -ESRCH;
        return;
    }

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
        if (!b.success()) {
            syscall_error(current) = b.result;
            return;
        }

        if (not b.val)
            return;
        break;
    }
    default:
        syscall_error(current) = -ENOTSUP;
        return;
    }

#ifdef __x86_64__
    if (target == current)
        restore_segments(target);
#endif
    syscall_success(current);
}

void syscall_get_segment()
{
    TaskDescriptor *current = get_cpu_struct()->current_task;

    u64 pid            = syscall_arg64(current, 0);
    ulong segment_type = syscall_arg(current, 1, 1);
    ulong ptr          = syscall_arg(current, 2, 1);

    syscall_success(current);

    TaskDescriptor *target {};
    u64 segment = 0;

    if (pid == 0 or current->task_id == pid)
        target = current;
    else
        target = get_task(pid);

    if (not target) {
        syscall_error(current) = -ESRCH;
        return;
    }

    switch (segment_type) {
    case 1: {
        segment = target->regs.thread_pointer();
        auto b =
            copy_to_user((char *)&current->regs.thread_pointer(), (char *)ptr, sizeof(segment));
        if (!b.success()) {
            syscall_error(current) = b.result;
            return;
        }

        if (not b.val)
            return;
        break;
    }
    case 2: {
        segment = target->regs.global_pointer();
        auto b =
            copy_to_user((char *)&current->regs.global_pointer(), (char *)ptr, sizeof(segment));
        if (!b.success()) {
            syscall_error(current) = b.result;
            return;
        }

        if (not b.val)
            return;
        break;
    }
    case 3: {
        // This is very bad (it overwrites user memory that is not supposed to be)...
        auto b = copy_to_user((char *)&target->regs, (char *)ptr, sizeof(target->regs));
        if (!b.success()) {
            syscall_error(current) = b.result;
            return;
        }

        if (not b.val)
            return;
        break;
    }
    default:
        syscall_error(current) = -ENOTSUP;
        return;
    }
}

void syscall_transfer_region()
{
    TaskDescriptor *current = get_current_task();

    u64 to_page_table = syscall_arg64(current, 0);
    ulong region      = syscall_arg(current, 1, 1);
    ulong dest        = syscall_arg(current, 2, 1);
    ulong flags       = syscall_flags(current);

    klib::shared_ptr<Arch_Page_Table> pt = Arch_Page_Table::get_page_table(to_page_table);
    if (!pt) {
        syscall_error(current) = -ENOENT;
        return;
    }

    bool fixed = flags & 0x08;

    const auto result = current->page_table->atomic_transfer_region(pt, region, dest, flags, fixed);
    if (!result.success()) {
        syscall_error(current) = result.result;
        return;
    }

    syscall_return(current) = result.val;
}

void syscall_asign_page_table()
{
    TaskDescriptor *current = get_current_task();

    u64 pid        = syscall_arg64(current, 0);
    u64 page_table = syscall_arg64(current, 1);
    ulong flags    = syscall_flags(current);

    TaskDescriptor *dest = get_task(pid);
    if (!dest) {
        syscall_error(current) = -ESRCH;
        return;
    }

    syscall_success(current);

    switch (flags) {
    case 1: { // PAGE_TABLE_CREATE
        auto result = dest->create_new_page_table();
        if (result) {
            syscall_error(current) = result;
        } else {
            syscall_return(current) = dest->page_table->id;
        }
        break;
    }
    case 2: // PAGE_TABLE_ASIGN
    {
        klib::shared_ptr<Arch_Page_Table> t;
        if (page_table == 0 or page_table == current->page_table->id)
            t = current->page_table;
        else
            t = Arch_Page_Table::get_page_table(page_table);

        if (!t) {
            syscall_error(current) = -ENOENT;
            return;
        }

        auto result =
            dest->atomic_register_page_table(klib::forward<klib::shared_ptr<Arch_Page_Table>>(t));
        if (result) {
            syscall_error(current) = result;
            return;
        } else {
            syscall_return(current) = dest->page_table->id;
        }
        break;
    }
    case 3: // PAGE_TABLE_CLONE
    {
        klib::shared_ptr<Arch_Page_Table> t = current->page_table->create_clone();
        auto result =
            dest->atomic_register_page_table(klib::forward<klib::shared_ptr<Arch_Page_Table>>(t));
        if (result) {
            syscall_error(current) = result;
        } else {
            syscall_return(current) = dest->page_table->id;
        }
        break;
    }
    default:
        syscall_error(current) = -ENOTSUP;
        break;
    }
}

void syscall_create_mem_object()
{
    const auto &current_task       = get_current_task();
    const auto &current_page_table = current_task->page_table;

    ulong size_bytes = syscall_arg(current_task, 0, 0);

    // TODO: Remove hard-coded page sizes

    // Syscall must be page aligned
    if (size_bytes & 07777) {
        syscall_error(current_task) = -EINVAL;
        return;
    }

    const auto page_size_log = 12; // 2^12 = 4096 bytes
    const auto size_pages    = size_bytes / 4096;

    const auto ptr = Mem_Object::create(page_size_log, size_pages);
    if (!ptr) {
        syscall_error(current_task) = -ENOMEM;
        return;
    }

    auto result = current_page_table->atomic_pin_memory_object(ptr);
    if (result) {
        syscall_error(current_task) = result;
        return;
    }

    syscall_return(current_task) = ptr->get_id();
}

void syscall_map_mem_object()
{
    const auto &current_task = get_current_task();
    u64 page_table_id        = syscall_arg64(current_task, 0);
    u64 object_id            = syscall_arg64(current_task, 1);
    u64 offset;
    auto result = syscall_arg64_checked(current_task, 2, offset);
    if (!result.success()) {
        syscall_error(current_task) = result.result;
        return;
    }
    if (!result.val)
        return;

    ulong args[3];
    result = syscall_args_checked(current_task, 3, 3, 3, args);
    if (!result.success()) {
        syscall_error(current_task) = result.result;
        return;
    }
    if (!result.val)
        return;

    ulong addr_start = args[0];
    ulong size_bytes = args[1];
    ulong access     = args[2];

    auto table = page_table_id == 0 ? current_task->page_table
                                    : Arch_Page_Table::get_page_table(page_table_id);

    if (!table) {
        syscall_error(current_task) = -ENOENT;
        return;
    }

    if ((size_bytes == 0) or ((size_bytes & 0xfff) != 0)) {
        syscall_error(current_task) = -EINVAL;
        return;
    }

    if (offset & 0xfff) {
        syscall_error(current_task) = -EINVAL;
        return;
    }

    const auto object = Mem_Object::get_object(object_id);
    if (!object) {
        syscall_error(current_task) = -ENOENT;
        return;
    }

    if (object->is_anonymous()) {
        syscall_error(current_task) = -EPERM;
        return;
    }

    if (object->size_bytes() < offset + size_bytes) {
        syscall_error(current_task) = -EFBIG;
        return;
    }

    auto res = table->atomic_create_mem_object_region(addr_start, size_bytes, access & 0x7,
                                                      access & 0x8, "object map", object,
                                                      access & 0x20, 0, offset, size_bytes);

    if (!res.success()) {
        syscall_error(current_task) = res.result;
        return;
    }

    syscall_return(current_task) = res.val->start_addr;
}

void syscall_delete_region()
{
    auto current_task  = get_current_task();
    u64 tid            = syscall_arg64(current_task, 0);
    ulong region_start = syscall_arg(current_task, 1, 1);

    syscall_success(current_task);

    // 0 is TASK_ID_SELF
    const auto task = tid == 0 ? get_cpu_struct()->current_task : get_task(tid);
    if (!task) {
        syscall_error(current_task) = -ESRCH;
        return;
    }

    {
        Auto_Lock_Scope lock(task->sched_lock);
        if (task->is_uninited()) {
            syscall_error(current_task) = -EBUSY;
            return;
        }
    }

    const auto &page_table = task->page_table;

    // TODO: Contemplate about adding a check to see if it's inside the kernels address space

    // TODO: This is completely untested and knowing me is probably utterly broken
    page_table->atomic_delete_region(region_start);
}

void syscall_unmap_range()
{
    auto current_task = get_current_task();

    u64 task_id      = syscall_arg64(current_task, 0);
    ulong addr_start = syscall_arg(current_task, 1, 1);
    ulong size       = syscall_arg(current_task, 2, 1);

    const auto task = task_id == 0 ? get_cpu_struct()->current_task : get_task(task_id);
    if (!task) {
        syscall_error(current_task) = -ESRCH;
        return;
    }

    const auto page_table = task->page_table;
    if (!page_table) {
        syscall_error(current_task) = -ENOENT;
        return;
    }

    // Maybe EINVAL if unaligned?
    auto offset             = addr_start & (PAGE_SIZE - 1);
    auto addr_start_aligned = addr_start - offset;
    auto size_aligned       = (size + offset + PAGE_SIZE - 1) & ~ulong(PAGE_SIZE - 1);

    syscall_error(current_task) =
        page_table->atomic_release_in_range(addr_start_aligned, size_aligned);
}

void syscall_remove_from_task_group()
{
    auto current_task = get_current_task();

    u64 pid   = syscall_arg64(current_task, 0);
    u64 group = syscall_arg64(current_task, 1);

    const auto task = pid == 0 ? get_current_task() : get_task(pid);
    if (!task) {
        syscall_error(current_task) = -ESRCH;
        return;
    }

    const auto group_ptr = TaskGroup::get_task_group(group);
    if (!group_ptr) {
        syscall_error(current_task) = -ENOENT;
        return;
    }

    syscall_error(current_task) = group_ptr->atomic_remove_task(task);
}

void syscall_create_task_group()
{
    const auto &current_task  = get_current_task();
    const auto new_task_group = TaskGroup::create();

    auto result = new_task_group->atomic_register_task(current_task);
    if (result) {
        syscall_error(current_task) = result;
        return;
    }

    syscall_return(current_task) = new_task_group->get_id();
}

void syscall_is_in_task_group()
{
    auto current_task = get_current_task();

    u64 pid   = syscall_arg64(current_task, 0);
    u64 group = syscall_arg64(current_task, 1);

    const u64 current_pid = pid == 0 ? get_current_task()->task_id : pid;
    const auto group_ptr  = TaskGroup::get_task_group(group);
    if (!group_ptr) {
        syscall_error(current_task) = -ENOENT;
        return;
    }

    const bool has_task          = group_ptr->atomic_has_task(current_pid);
    syscall_return(current_task) = has_task;
}

void syscall_add_to_task_group()
{
    auto current_task = get_current_task();

    u64 pid   = syscall_arg64(current_task, 0);
    u64 group = syscall_arg64(current_task, 1);

    const auto task = pid == 0 ? current_task : get_task(pid);
    if (!task) {
        syscall_error(current_task) = -ESRCH;
        return;
    }

    const auto group_ptr = TaskGroup::get_task_group(group);
    if (!group_ptr) {
        syscall_error(current_task) = -ENOENT;
        return;
    }

    syscall_error(current_task) = group_ptr->atomic_register_task(task);
}

void syscall_set_notify_mask()
{
    auto current_task = get_current_task();

    u64 task_group = syscall_arg64(current_task, 0);
    u64 port_id    = syscall_arg64(current_task, 1);
    ulong new_mask;
    {
        auto result = syscall_args_checked(current_task, 2, 2, 1, &new_mask);
        if (!result.success()) {
            syscall_error(current_task) = result.result;
            return;
        }
        if (!result.val)
            return;
    }
    ulong flags = syscall_flags(current_task);

    const auto group = TaskGroup::get_task_group(task_group);
    if (!group) {
        syscall_error(current_task) = -ENOENT;
        return;
    }

    const auto port = Port::atomic_get_port(port_id);
    if (!port) {
        syscall_error(current_task) = -ENOENT;
        return;
    }

    auto old_mask = group->atomic_change_notifier_mask(port, new_mask, flags);
    if (!old_mask.success()) {
        syscall_error(current_task) = old_mask.result;
    } else {
        syscall_return(current_task) = old_mask.val;
    }
}

u64 get_current_time_ticks();

void syscall_request_timer()
{
    auto c    = get_cpu_struct();
    auto task = c->current_task;

    u64 port     = syscall_arg64(task, 0);
    u64 timeout  = syscall_arg64(task, 1);
    u64 user_arg = syscall_arg64(task, 2);

    const auto port_ptr = Port::atomic_get_port(port);
    if (!port_ptr) {
        syscall_error(task) = -ENOENT;
        return;
    }

    u64 core_time_ms    = c->ticks_after_ns(timeout);
    syscall_error(task) = c->atomic_timer_queue_push(core_time_ms, port_ptr, user_arg);
}

void syscall_set_affinity()
{
    const auto current_cpu = get_cpu_struct();
    auto current_task      = current_cpu->current_task;

    auto pid          = syscall_arg64(current_task, 0);
    uint32_t affinity = syscall_arg(current_task, 1, 1);
    ulong flags       = syscall_flags(current_task);

    const auto task = pid == 0 ? current_cpu->current_task : get_task(pid);
    if (!task) {
        syscall_error(current_task) = -ESRCH;
        return;
    }

    const auto cpu       = affinity == -1U ? current_cpu->cpu_id + 1 : affinity;
    const auto cpu_count = get_cpu_count();
    if (cpu > cpu_count) {
        syscall_error(current_task) = -EINVAL;
        return;
    }

    if (task != current_cpu->current_task) {
        Auto_Lock_Scope lock(task->sched_lock);
        if (task->status != TaskStatus::TASK_PAUSED) {
            syscall_error(current_task) = -EBUSY;
            return;
        }

        if (not task->can_be_rebound()) {
            syscall_error(current_task) = -EPERM;
            return;
        }

        syscall_return(current_task) = task->cpu_affinity;
        task->cpu_affinity   = cpu;
    } else {
        if (not task->can_be_rebound()) {
            syscall_return(current_task) = -EPERM;
            return;
        }

        if (cpu != 0 && cpu != (current_cpu->cpu_id + 1)) {
            syscall_success(current_task);
            find_new_process();

            {
                Auto_Lock_Scope lock(task->sched_lock);
                syscall_return(task) = task->cpu_affinity;
                task->cpu_affinity   = cpu;
                push_ready(task);
            }

            auto remote_cpu = cpus[cpu - 1];
            assert(remote_cpu->cpu_id == cpu - 1);
            if (remote_cpu->current_task_priority > task->priority)
                remote_cpu->ipi_reschedule();
        } else {
            Auto_Lock_Scope lock(task->sched_lock);
            syscall_return(current_task) = task->cpu_affinity;
            task->cpu_affinity   = cpu;
        }
    }

    // serial_logger.printf("Task %d (%s) affinity set to %d\n", task->task_id, task->name.c_str(),
    // cpu);

    reschedule();
}

void syscall_yield()
{
    syscall_success(get_current_task());
    reschedule();
}

void syscall_get_time()
{
    const auto current_task = get_current_task();
    ulong mode              = syscall_arg(current_task, 0, 0);

    switch (mode) {
    case GET_TIME_NANOSECONDS_SINCE_BOOTUP:
        syscall_return(current_task) = get_ns_since_bootup();
        break;
    case GET_TIME_REALTIME_NANOSECONDS:
        syscall_return(current_task) = unix_time_bootup * 1000000000 + get_ns_since_bootup();
        break;
    default:
        syscall_error(current_task) = -ENOTSUP;
        break;
    }
}

void syscall_system_info()
{
    const auto current_task = get_current_task();
    ulong param             = syscall_arg(current_task, 0, 0);

    switch (param) {
    case SYSINFO_NPROCS:
    case SYSINFO_NPROCS_CONF:
        syscall_return(current_task) = get_cpu_count();
        break;
    default:
        syscall_error(current_task) = -ENOTSUP;
        break;
    }
}

void syscall_pause_task()
{
    const auto current_task = get_current_task();
    if (current_task->regs.syscall_pending_restart())
        current_task->pop_repeat_syscall();

    auto task_id = syscall_arg64(current_task, 0);

    const auto task = task_id == 0 ? current_task : get_task(task_id);
    if (!task) {
        syscall_error(current_task) = -ESRCH;
        return;
    }

    if (task->is_kernel_task()) {
        syscall_error(current_task) = -EPERM;
    }

    Auto_Lock_Scope lock(task->sched_lock);
    switch (task->status) {
    case TaskStatus::TASK_RUNNING: {
        if (current_task == task) {
            task->status = TaskStatus::TASK_PAUSED;

            syscall_success(current_task);

            task->parent_queue = &paused;
            {
                Auto_Lock_Scope lock(paused.lock);
                paused.push_back(task);
            }
            find_new_process();
        } else {
            {
                Auto_Lock_Scope lock(current_task->sched_lock);
                if (current_task->status == TaskStatus::TASK_DYING) {
                    syscall_error(current_task) = -ESRCH;
                    return;
                }

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
        syscall_success(current_task);
        return;
    case TaskStatus::TASK_READY: {
        task->status = TaskStatus::TASK_PAUSED;
        task->parent_queue->atomic_erase(task);

        syscall_success(current_task);

        task->parent_queue = &paused;
        Auto_Lock_Scope lock(paused.lock);
        paused.push_back(task);
        break;
    }
    case TaskStatus::TASK_UNINIT:
        syscall_error(current_task) = -EINVAL;
        return;
    case TaskStatus::TASK_SPECIAL:
        syscall_error(current_task) = -EPERM;
        return;
    case TaskStatus::TASK_DYING:
    case TaskStatus::TASK_DEAD:
        syscall_error(current_task) = -ESRCH;
        return;
    case TaskStatus::TASK_BLOCKED: {
        if (task->regs.syscall_pending_restart())
            task->interrupt_restart_syscall();

        task->status = TaskStatus::TASK_PAUSED;
        task->parent_queue->atomic_erase(task);

        syscall_success(current_task);

        task->parent_queue = &paused;
        Auto_Lock_Scope lock(paused.lock);
        paused.push_back(task);
        break;
    }
    }
}

void syscall_resume_task()
{
    // TODO: Start task syscall is redundant
    const auto current_task = get_current_task();
    auto task_id            = syscall_arg64(current_task, 0);

    const auto task = get_task(task_id);
    if (!task) {
        syscall_error(current_task) = -ESRCH;
        return;
    }

    Auto_Lock_Scope lock(task->sched_lock);
    syscall_success(current_task);

    if (task->is_uninited() or task->status == TaskStatus::TASK_PAUSED)
        task->init();
    else if (task->status != TaskStatus::TASK_RUNNING) {
        syscall_error(current_task) = -EBUSY;
        return;
    }
}

void syscall_get_page_address()
{
    auto current_task = get_current_task();
    auto task_id      = syscall_arg64(current_task, 0);
    auto page_base    = syscall_arg(current_task, 1, 1);
    auto flags        = syscall_flags(current_task);

    if (page_base & (PAGE_SIZE - 1)) {
        syscall_error(current_task) = -EINVAL;
        return;
    }

    auto task = task_id == 0 ? get_current_task() : get_task(task_id);
    if (!task) {
        syscall_error(current_task) = -ESRCH;
        return;
    }

    if (task->status == TaskStatus::TASK_UNINIT) {
        syscall_error(current_task) = -EBUSY;
        return;
    }

    syscall_success(task);

    auto table = task->page_table;
    assert(table);

    Auto_Lock_Scope lock(table->lock);

    auto b = table->prepare_user_page(page_base, 0);
    if (!b.success()) {
        syscall_error(task) = b.result;
        return;
    }

    if (not b.val)
        return;

    auto mapping         = table->get_page_mapping(page_base);
    syscall_return(task) = mapping.page_addr;
}