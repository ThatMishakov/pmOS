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

#include <array>
#include <assert.h>
#include <clock.hh>
#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>
#include <lib/string.hh>
#include <lib/utility.hh>
#include <memory/mem_object.hh>
#include <pmos/system.h>
#include <sched/sched.hh>

#if defined(__x86_64__) || defined(__i386__)
    #include <sched/segments.hh>
#endif

using namespace kernel::log;
using namespace kernel::sched;
using namespace kernel::ipc;
using namespace kernel::paging;
using namespace kernel::interrupts;

ReturnStr<u32> acpi_wakeup_vec();
extern void stop_cpus();
extern void deactivate_page_table();

namespace kernel::proc::syscalls
{

using syscall_function                         = void (*)();
std::array<syscall_function, 55> syscall_table = {
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
    send_message_right,
    syscall_send_message_port,
    syscall_create_port,
    syscall_set_attribute,
    syscall_set_interrupt,
    syscall_create_right,
    syscall_set_namespace,
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
    syscall_create_mem_object,
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
    syscall_unreference_mem_object,
    syscall_get_page_address_from_object,
    nullptr,
    syscall_cpu_for_interrupt,
    syscall_set_port0,
    syscall_delete_send_right,
};

extern "C" void syscall_handler()
{
    TaskDescriptor *task = sched::get_cpu_struct()->current_task;

    unsigned call_n = syscall_number(task);

    // The syscall number is sometimes overwritten which causes issues
    // This is a "temporary" workaround
    task->syscall_num = syscall_flags_reg(task);

    // serial_logger.printf("syscall_handler: task: %d (%s) call_n: %x\n", task->task_id,
    // task->name.c_str(), call_n);

    // TODO: check permissions

    // t_print_bochs("Debug: syscall %h pid %h (%s) ", call_n,
    // sched::get_cpu_struct()->current_task->task_id,
    // sched::get_cpu_struct()->current_task->name.c_str()); t_print_bochs(" %h %h %h %h %h ", arg1,
    // arg2, arg3, arg4, arg5);
    if (task->attr.debug_syscalls) {
        serial_logger.printf("Debug: syscall %h pid %h\n", call_n,
                             sched::get_cpu_struct()->current_task->task_id);
    }

    // TODO: This crashes if the syscall is not implemented
    if (call_n >= syscall_table.size() or syscall_table[call_n] == nullptr) {
        serial_logger.printf("Debug: syscall %h pid %li (%s) ", call_n, task->task_id,
                             task->name.c_str());
        serial_logger.printf(" -> %i (%s)\n", -ENOTSUP, "syscall not implemented");
        syscall_error(task) = -ENOTSUP;
        return;
    }

    syscall_table[call_n]();

    if ((syscall_error(task) < 0) && !task->regs.syscall_pending_restart()) {
        serial_logger.printf("Debug: syscall %h (%i) pid %li (%s) ", call_n, call_n, task->task_id,
                             task->name.c_str());
        int val = syscall_error(task);
        serial_logger.printf(" -> %i (%s)\n", val, "syscall failed");
    }

    // t_print_bochs("SUCCESS %h\n", syscall_ret_high(task));

    // t_print_bochs(" -> SUCCESS\n");
}

void syscall_get_task_id()
{
    const task_ptr &task = sched::get_cpu_struct()->current_task;

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
    TaskDescriptor *task = sched::get_cpu_struct()->current_task;

    ulong arg1 = syscall_arg(task, 0, 0);
    ulong arg2 = syscall_arg(task, 1, 0);

    // serial_logger.printf("syscall exit task %li (%s) arg %x\n", task->task_id,
    // task->name.c_str(), arg1);

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
    TaskDescriptor *current = sched::get_cpu_struct()->current_task;

    u64 portno = syscall_arg64(current, 0);
    ulong buff = syscall_arg(current, 1, 1);
    ulong args = syscall_flags(current);

    auto port = Port::atomic_get_port(portno);
    if (!port) {
        syscall_error(current) = -ENOENT;
        return;
    }

    Message *top_message = nullptr;

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
    }

    syscall_success(current);
    auto result = top_message->copy_to_user_buff((char *)buff);
    if (!result.success()) {
        syscall_error(current) = result.result;
        return;
    }

    if (not result.val)
        return;

    u64 reply_right_id = 0;
    if (!(args & MSG_ARG_NOPOP)) {
        if (!(args & MSG_ARG_REJECT_RIGHT) && top_message->reply_right) {
            auto right = top_message->reply_right;
            auto group = current->get_rights_namespace();
            if (!group) {
                syscall_error(current) = -ESRCH;
                return;
            }

            reply_right_id = group->atomic_new_right_id();

            Auto_Lock_Scope l(right->lock);
            if (right->alive) {
                Auto_Lock_Scope gl(group->rights_lock);

                if (!group->atomic_alive()) {
                    syscall_error(current) = -ESRCH;
                    return;
                }

                right->right_sender_id = reply_right_id;
                group->rights.insert(right);
                right->of_message = false;

                top_message->reply_right = nullptr;
            }
        }

        Auto_Lock_Scope scope_lock(port->lock);
        port->pop_front();
    }

    syscall_return(current) = reply_right_id;
}

void syscall_send_message_port()
{
    static constexpr unsigned flag_send_extended = 1 << 8;
    TaskDescriptor *current                      = sched::get_cpu_struct()->current_task;

    u64 port_num   = syscall_arg64(current, 0);
    ulong size     = syscall_arg(current, 1, 1);
    ulong message  = syscall_arg(current, 2, 1);
    u64 mem_object = 0;
    ulong flags    = syscall_flags(current);

    if (flags & flag_send_extended) {
        mem_object = syscall_arg64(current, 1);

        auto result = syscall_args_checked(current, 2, 2, 1, &size);
        if (!result.success()) {
            syscall_error(current) = result.result;
            return;
        }
        if (!result.val) {
            return;
        }

        result = syscall_args_checked(current, 3, 2, 1, &message);
        if (!result.success()) {
            syscall_error(current) = result.result;
            return;
        }
        if (!result.val) {
            return;
        }
    }

    // TODO: Check permissions

    auto port = Port::atomic_get_port(port_num);
    if (!port) {
        syscall_error(current) = -ENOENT;
        return;
    }

    syscall_success(current);
    auto result = port->atomic_send_from_user(current, (char *)message, size, mem_object);
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

    Message *msg {};

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

    bool holds_reply_right     = msg->reply_right;
    bool reply_right_send_many = false;
    if (holds_reply_right)
        reply_right_send_many = msg->reply_right->type == RightType::SendMany;

    u64 msg_struct_size     = sizeof(Message_Descriptor);
    Message_Descriptor desc = {
        .sender             = msg->task_id_from,
        .mem_object         = msg->mem_object_id,
        .size               = msg->size(),
        .sent_with_right    = msg->sent_with_right,
        .other_rights_count = (unsigned)msg->rights_count(),
        .flags              = (holds_reply_right ? (unsigned)MESSAGE_FLAG_REPLY_RIGHT : 0) |
                 (reply_right_send_many ? (unsigned)MESSAGE_FLAG_REPLY_SEND_MANY : 0),
    };

    syscall_success(task);
    auto b = copy_to_user((char *)&desc, (char *)message_struct, msg_struct_size);
    if (!b.success()) {
        syscall_error(task) = b.result;
        return;
    }

    if (not b.val)
        assert(!"blocking by page is not yet implemented");
} // namespace kernel::proc::syscalls

void syscall_set_attribute()
{
    auto c        = sched::get_cpu_struct();
    task_ptr task = c->current_task;

    u64 pid         = syscall_arg64(task, 0);
    ulong attribute = syscall_arg(task, 1, 1);
    ulong value     = syscall_arg(task, 2, 1);

    // TODO: Check persmissions

    // TODO: This is *very* x86-specific and is a bad idea in general

#if defined(__x86_64__) || defined(__i386__)
    TaskDescriptor *process = pid == 0 || pid == task->task_id ? task : get_task(pid);
    if (!process) {
        syscall_error(task) = -ESRCH;
        return;
    }

    switch (attribute) {
    case ATTR_ALLOW_PORT:
        syscall_return(task) = 0;
        process->regs.set_iopl(value ? 3 : 0);
        break;

    case ATTR_DEBUG_SYSCALLS:
        process->attr.debug_syscalls = value;
        break;

    case 3: // ACPI sleep states' stuff
        if (process != task) {
            syscall_error(task) = -EINVAL;
            return;
        }

        syscall_return(task) = 0;

        c->to_restore_on_wakeup = klib::make_unique<Task_Regs>(task->regs);
        if (!c->to_restore_on_wakeup) {
            syscall_error(task) = -ENOMEM;
            return;
        }
        stop_cpus();
        task->page_table->unapply_cpu(get_cpu_struct());
        task->before_task_switch();
        task->regs.entry_type = 5;
        break;

    case 4: {
        auto result = acpi_wakeup_vec();
        if (!result.success()) {
            syscall_error(task) = result.result;
        } else {
            syscall_return(task) = result.val;
        }
    } break;

    case 5: { // Number of NVS entries
        // TODO: Remove magic numbers
        size_t count = 0;
        for (auto region: memory_map)
            count += region.type == MemoryRegionType::ACPINVS;

        syscall_return(task) = count;
    } break;

    case 6: { // Return NVS regions to userspace...
        klib::vector<MemoryRegion> nvs_regions;
        for (auto region: memory_map)
            if (region.type == MemoryRegionType::ACPINVS && !nvs_regions.push_back(region)) {
                syscall_error(task) = -ENOMEM;
                return;
            }

        char *user_ptr = reinterpret_cast<char *>(value);
        syscall_success(task);
        auto copy_result = copy_to_user((char *)&nvs_regions[0], user_ptr,
                                        sizeof(nvs_regions[0]) * nvs_regions.size());
        if (!copy_result.success()) {
            syscall_error(task) = copy_result.result;
            return;
        }

        if (!copy_result.val)
            return;
    } break;

    case 7:
        syscall_success(task);
        deactivate_page_table();
        __asm__ volatile("wbinvd");
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
#if defined(__x86_64__) || defined(__i386__)

    CPU_Info *i;
    if (cpu_id == 0) {
        i = sched::get_cpu_struct();
    } else if (cpu_id > cpus.size()) {
        syscall_error(current_task) = -ENOENT;
        return;
    } else
        i = cpus[cpu_id - 1];

    // TODO: Store lapic_id withouth shifting it
    syscall_return(current_task) = i->lapic_id;

#else
    // Not available on RISC-V
    // TODO: This should be hart id
    // syscall_ret_high(get_current_task()) = sched::get_cpu_struct()->lapic_id;
    syscall_error(current_task) = -ENOSYS;
#endif
}

void syscall_set_task_name()
{
    task_ptr current = sched::get_cpu_struct()->current_task;

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
#if defined(__riscv) || defined(__x86_64__) || defined(__i386__) || defined(__loongarch__)
    auto c               = sched::get_cpu_struct();
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
#if defined(__riscv) || defined(__x86_64__) || defined(__i386__) || defined(__loongarch__)
    auto c               = sched::get_cpu_struct();
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
    TaskDescriptor *current = sched::get_cpu_struct()->current_task;

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
        (void *)addr_start, size, access & 0x07, access & 0x08, access & 0x10,
        klib::move(region_name), 0, access & 0x20);
    if (!result.success()) {
        syscall_error(current) = result.result;
    } else {
        syscall_return(current) = (ulong)result.val->start_addr;
    }
}

void syscall_create_phys_map_region()
{
    TaskDescriptor *current = sched::get_cpu_struct()->current_task;
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
    if ((addr_start & 07777) or (size & 07777) or (phys_addr & 07777)) {
        syscall_error(current) = -EINVAL;
        return;
    }

    auto result = dest_task->page_table->atomic_create_phys_region(
        (void *)addr_start, size, access & 0x07, access & 0x08, klib::string(), phys_addr);
    if (!result.success()) {
        syscall_error(current) = result.result;
    } else {
        syscall_return(current) = (ulong)result.val->start_addr;
    }
}

void syscall_get_page_table()
{
    TaskDescriptor *current = sched::get_cpu_struct()->current_task;

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
    TaskDescriptor *current = sched::get_cpu_struct()->current_task;
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
#if defined(__x86_64__) || defined(__i386__)
    case 2:
        target->regs.global_pointer() = ptr;
        break;
#endif
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

#if defined(__x86_64__) || defined(__i386__)
    if (target == current)
        restore_segments(target);
#endif
    syscall_success(current);
}

void syscall_get_segment()
{
    TaskDescriptor *current = sched::get_cpu_struct()->current_task;

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
    // case 2: {
    //     segment = target->regs.global_pointer();
    //     auto b =
    //         copy_to_user((char *)&current->regs.global_pointer(), (char *)ptr, sizeof(segment));
    //     if (!b.success()) {
    //         syscall_error(current) = b.result;
    //         return;
    //     }

    //     if (not b.val)
    //         return;
    //     break;
    // }
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

    auto pt = Arch_Page_Table::get_page_table(to_page_table);
    if (!pt) {
        syscall_error(current) = -ENOENT;
        return;
    }

    bool fixed = flags & 0x08;

    const auto result =
        current->page_table->atomic_transfer_region(pt, (void *)region, (void *)dest, flags, fixed);
    if (!result.success()) {
        syscall_error(current) = result.result;
        return;
    }

    syscall_return(current) = (ulong)result.val;
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
    ulong flags      = syscall_flags(current_task);

    // TODO: Remove hard-coded page sizes

    // Syscall must be page aligned
    if (size_bytes & 07777) {
        syscall_error(current_task) = -EINVAL;
        return;
    }

    const auto page_size_log = 12; // 2^12 = 4096 bytes
    const auto size_pages    = size_bytes / 4096;

    const auto ptr = Mem_Object::create(page_size_log, size_pages, flags);
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
    ulong access             = syscall_flags(current_task);
    u64 offset;
    auto result = syscall_arg64_checked(current_task, 2, offset);
    if (!result.success()) {
        syscall_error(current_task) = result.result;
        return;
    }
    if (!result.val)
        return;

    ulong args[2];
    result = syscall_args_checked(current_task, 3, 3, 2, args);
    if (!result.success()) {
        syscall_error(current_task) = result.result;
        return;
    }
    if (!result.val)
        return;

    ulong addr_start = args[0];
    ulong size_bytes = args[1];

    klib::shared_ptr<Page_Table> table = page_table_id == 0
                                             ? current_task->page_table
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

    auto res = table->atomic_create_mem_object_region((void *)addr_start, size_bytes, access & 0x7,
                                                      access & 0x8, "object map", object,
                                                      access & 0x20, 0, offset, size_bytes);

    if (!res.success()) {
        syscall_error(current_task) = res.result;
        return;
    }

    syscall_return(current_task) = (ulong)res.val->start_addr;
}

void syscall_delete_region()
{
    auto current_task  = get_current_task();
    u64 tid            = syscall_arg64(current_task, 0);
    ulong region_start = syscall_arg(current_task, 1, 1);

    syscall_success(current_task);

    // 0 is TASK_ID_SELF
    const auto task = tid == 0 ? sched::get_cpu_struct()->current_task : get_task(tid);
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
    page_table->atomic_delete_region((void *)region_start);
}

void syscall_unmap_range()
{
    auto current_task = get_current_task();

    u64 task_id      = syscall_arg64(current_task, 0);
    ulong addr_start = syscall_arg(current_task, 1, 1);
    ulong size       = syscall_arg(current_task, 2, 1);

    const auto task = task_id == 0 ? sched::get_cpu_struct()->current_task : get_task(task_id);
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
        page_table->atomic_release_in_range((void *)addr_start_aligned, size_aligned);
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
    const auto &current_task = get_current_task();
    const auto g             = TaskGroup::create_for_task(current_task);

    if (g.success())
        syscall_return(current_task) = g.val->get_id();
    else
        syscall_error(current_task) = g.result;
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
    auto c    = sched::get_cpu_struct();
    auto task = c->current_task;

    u64 port    = syscall_arg64(task, 0);
    u64 timeout = syscall_arg64(task, 1);
    u64 user_arg;
    auto result = syscall_arg64_checked(task, 2, user_arg);
    if (!result.success()) {
        syscall_error(task) = result.result;
        return;
    }

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
    const auto current_cpu = sched::get_cpu_struct();
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
        task->cpu_affinity           = cpu;
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
            task->cpu_affinity           = cpu;
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

    auto b = table->prepare_user_page((void *)page_base, 0);
    if (!b.success()) {
        syscall_error(task) = b.result;
        return;
    }

    if (not b.val)
        return;

    auto mapping         = table->get_page_mapping((void *)page_base);
    syscall_return(task) = mapping.page_addr;
}

void syscall_get_page_address_from_object()
{
    auto current_task = get_current_task();
    auto object_id    = syscall_arg64(current_task, 0);
    auto offset       = syscall_arg64(current_task, 1);

    auto object = Mem_Object::get_object(object_id);
    if (!object) {
        syscall_error(current_task) = -ENOENT;
        return;
    }

    if (object->is_anonymous()) {
        syscall_error(current_task) = -EPERM;
        return;
    }

    auto page = object->atomic_request_page(offset, true, false);
    if (!page.success()) {
        syscall_error(current_task) = page.result;
        return;
    }

    if (!page.val) {
        // TODO
        serial_logger.printf(
            "syscall_get_page_address_from_object hit unimpmemented stuff, failing...\n");
        syscall_error(current_task) = -ENOSYS;
        return;
    }

    syscall_return(current_task) = page.val.get_phys_addr();
}

void syscall_unreference_mem_object()
{
    auto current_task = get_current_task();
    auto object_id    = syscall_arg64(current_task, 0);

    auto object = Mem_Object::get_object(object_id);
    if (!object) {
        syscall_error(current_task) = -ENOENT;
        return;
    }

    auto result = current_task->page_table->atomic_unpin_memory_object(object);
    if (result) {
        syscall_error(current_task) = result;
        return;
    }
    syscall_success(current_task);
}

void syscall_cpu_for_interrupt()
{
    auto current_task = get_current_task();
    auto gsi          = syscall_arg(current_task, 0, 0);
    auto flags        = syscall_flags(current_task);

    bool edge       = !(flags & 0x01);
    bool active_low = (flags & 0x02);

    auto result = allocate_interrupt_single(gsi, edge, active_low);
    if (!result.success()) {
        syscall_error(current_task) = result.result;
        return;
    }

    assert(result.val.first);
    syscall_return(current_task) = (result.val.first->cpu_id + 1) | (uint64_t)(result.val.second)
                                                                        << 32;
}

void syscall_set_port0()
{
    auto current = get_current_task();
    u64 portno   = syscall_arg64(current, 0);

    auto port = Port::atomic_get_port(portno);
    if (!port) {
        syscall_error(current) = -ENOENT;
        return;
    }

    auto result = set_port0(port);
    if (result)
        syscall_error(current) = result;
    else
        syscall_success(current);
}

void syscall_set_namespace()
{
    auto current = get_current_task();

    u64 id        = syscall_arg64(current, 0);
    unsigned type = syscall_arg(current, 1, 1);
    switch (type) {
    case NAMESPACE_RIGHTS: {
        auto old_namespace = current->rights_namespace.load(std::memory_order::consume);
        auto group         = TaskGroup::get_task_group(id);
        if (!group) {
            syscall_error(current) = -ESRCH;
            return;
        }

        {
            Auto_Lock_Scope l(group->tasks_lock);
            if (!group->alive()) {
                syscall_error(current) = -ESRCH;
                return;
            }

            if (!group->task_in_group(current->task_id)) {
                syscall_error(current) = -ESRCH;
                return;
            }

            current->rights_namespace.store(group, std::memory_order::release);
        }

        syscall_return(current) = old_namespace ? old_namespace->get_id() : 0;
    } break;
    default:
        syscall_error(current) = -EINVAL;
    }
}

void syscall_create_right()
{
    auto current = get_current_task();

    u64 port_id    = syscall_arg64(current, 0);
    ulong ptr      = syscall_arg(current, 1, 1);
    unsigned flags = syscall_flags(current);

    auto group = current->rights_namespace.load(std::memory_order::consume);
    if (!group) {
        syscall_error(current) = -ESRCH;
        return;
    }

    auto port = Port::atomic_get_port(port_id);
    if (!port) {
        syscall_error(current) = -ESRCH;
        return;
    }

    if (port->owner != current) {
        syscall_error(current) = -EPERM;
        return;
    }

    auto right_id = port->new_right_id();
    if (ptr) {
        auto b = copy_to_user((const char *)&right_id, (char *)ptr, sizeof(right_id));
        if (!b.success()) {
            syscall_error(current) = b.result;
            return;
        }

        if (not b.val)
            return;
    }

    bool send_once = flags & CREATE_RIGHT_SEND_ONCE;
    RightType type = send_once ? RightType::SendOnce : RightType::SendMany;
    auto result    = Right::create_for_group(port, group, type, right_id);
    if (!result.success()) {
        syscall_error(current) = result.result;
        return;
    }

    syscall_return(current) = result.val->right_sender_id;
}

void send_message_right()
{
    auto current = get_current_task();

    u64 right_id      = syscall_arg64(current, 0);
    u64 reply_port_id = syscall_arg64(current, 1);
    auto flags        = syscall_flags(current);

    ulong args[3];
    auto result = syscall_args_checked(current, 2, 2, 3, args);
    if (!result.success()) {
        syscall_error(current) = result.result;
        return;
    }

    if (!result.val)
        return;

    auto [message, size, aux_data] = args;
    auto buffer                    = to_buffer_from_user((void *)message, size);
    if (!buffer.success()) {
        syscall_error(current) = result.result;
        return;
    }

    if (!buffer.val)
        return;

    auto group = current->rights_namespace.load(std::memory_order::consume);
    if (!group) {
        syscall_error(current) = -ESRCH;
        return;
    }

    auto right = group->atomic_get_right(right_id);
    if (!right) {
        syscall_error(current) = -ESRCH;
        return;
    }

    Port *reply_port = nullptr;
    if (reply_port_id) {
        reply_port = Port::atomic_get_port(reply_port_id);
        if (!reply_port) {
            syscall_error(current) = -ENOENT;
            return;
        }

        if (reply_port->owner != current) {
            syscall_error(current) = -EPERM;
            return;
        }
    }

    ipc::rights_array rights = {};
    if (aux_data) {
        message_extra_t d;
        auto result = copy_from_user((char *)&d, (char *)aux_data, sizeof(d));
        if (!result.success()) {
            syscall_error(current) = result.result;
            return;
        }

        if (!result.val)
            return;

        for (auto i = 0; i < 4; ++i) {
            if (auto id = d.extra_rights[i]; id) {
                auto right = group->atomic_get_right(id);
                if (!right) {
                    syscall_error(current) = -ESRCH;
                    return;
                }

                rights[i] = right;
            }
        }
    }

    auto new_type      = flags & REPLY_CREATE_SEND_MANY ? RightType::SendMany : RightType::SendOnce;
    bool always_delete = flags & SEND_MESSAGE_DELETE_RIGHT;

    auto send_result =
        Port::send_message_right(right, group, reply_port, rights, std::move(*buffer.val),
                                 current->task_id, new_type, always_delete);
    if (!send_result.success()) {
        syscall_error(current) = send_result.result;
        return;
    }

    syscall_return(current) = send_result.val ? send_result.val->right_parent_id : 0;
}

void syscall_delete_send_right()
{
    auto current = get_current_task();
    u64 right_id = syscall_arg64(current, 0);

    auto group = current->get_rights_namespace();
    if (!group) {
        syscall_error(current) = -ESRCH;
        return;
    }

    auto right = group->atomic_get_right(right_id);
    if (!right) {
        syscall_error(current) = -ESRCH;
        return;
    }

    auto result = right->destroy(group);
    if (result) {
        syscall_success(current);
    } else {
        syscall_error(current) = -ESRCH;
    }
}

} // namespace kernel::proc::syscalls