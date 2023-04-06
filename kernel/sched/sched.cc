#include "sched.hh"
#include <asm.hh>
#include <memory/paging.hh>
#include <types.hh>
#include <kernel/errors.h>
#include <linker.hh>
#include "processes/idle.hh"
#include <asm.hh>
#include <kernel/com.h>
#include <kernel/block.h>
#include <misc.hh>
#include <interrupts/apic.hh>
#include <cpus/cpus.hh>
#include <lib/memory.hh>
#include "timers.hh"
#include <cpus/sse.hh>
#include <interrupts/apic.hh>
#include <cpus/cpus.hh>
#include <stdlib.h>
#include <assert.h>

sched_queue blocked;
sched_queue uninit;
sched_queue dead_queue;

Spinlock tasks_map_lock;
sched_map tasks_map;

PID pid = 1;

void init_scheduling()
{
    init_per_cpu();
    
    klib::shared_ptr<TaskDescriptor> current_task = TaskDescriptor::create();
    get_cpu_struct()->current_task = current_task;

    current_task->register_page_table(x86_4level_Page_Table::init_from_phys(getCR3()));

    try {
        current_task->page_table->atomic_create_phys_region(0x1000, GB(4), Page_Table::Readable | Page_Table::Writeable | Page_Table::Executable, true, "init_default_map", 0x1000);

        current_task->pid = pid++;    
        tasks_map.insert({current_task->pid, klib::move(current_task)});
    } catch (const Kern_Exception& e) {
        t_print_bochs("Error: Could not assign the page table to the first process. Error %i (%s)\n", e.err_code, e.err_message);
        throw;
    }
}

DECLARE_LOCK(assign_pid);

PID assign_pid()
{
    LOCK(assign_pid)

    PID pid_p = pid++;

    UNLOCK(assign_pid)

   return pid_p; 
}

ReturnStr<u64> block_current_task(const klib::shared_ptr<Generic_Port>& ptr)
{
    const klib::shared_ptr<TaskDescriptor>& task = get_cpu_struct()->current_task;

    //t_print_bochs("Blocking %i (%s) by port\n", task->pid, task->name.c_str());

    Auto_Lock_Scope scope_lock(task->sched_lock);

    task->status = Process_Status::PROCESS_BLOCKED;
    task->blocked_by = ptr;
    task->parent_queue = &blocked;

    {
        Auto_Lock_Scope scope_l(blocked.lock);
        blocked.push_back(task);
    }

    // Task switch
    find_new_process();

    return {SUCCESS, 0};
}


void TaskDescriptor::atomic_block_by_page(u64 page)
{
    if (status == PROCESS_BLOCKED)
        throw(Kern_Exception(ERROR_ALREADY_BLOCKED, "atomic_block_by_page task already blocked"));

    // t_print_bochs("Blocking %i (%s) by page. CPU %i\n", this->pid, this->name.c_str(), get_cpu_struct()->cpu_id);

    Auto_Lock_Scope scope_lock(sched_lock);
    
    status = Process_Status::PROCESS_BLOCKED;
    page_blocked_by = page;

    klib::shared_ptr<TaskDescriptor> self = weak_self.lock();

    if (get_cpu_struct()->current_task == self) {
        find_new_process();
    } else if (parent_queue) {
        Auto_Lock_Scope scope_l(parent_queue->lock);
        parent_queue->erase(self);
    }
    
    {
        Auto_Lock_Scope scope_l(blocked.lock);
        blocked.push_back(self);
    }
}

void TaskDescriptor::switch_to()
{
    //t_print_bochs("[Kernel] Debug: Switching to PID %i (%s) CPU %i\n", pid, name.c_str(), get_cpu_struct()->cpu_id);

    CPU_Info *c = get_cpu_struct();
    if (c->current_task->page_table != page_table) {
        setCR3(klib::dynamic_pointer_cast<x86_Page_Table>(page_table)->get_cr3());
    }

    save_segments(c->current_task);

    if (sse_is_valid()) {
        //t_print_bochs("Saving SSE registers for PID %h\n", c->current_task->pid);
        c->current_task->sse_data.save_sse();
        invalidate_sse();
    }

    // Change task
    status = Process_Status::PROCESS_RUNNING;
    c->current_task_priority = priority;
    c->current_task = weak_self.lock();

    restore_segments(c->current_task);

    start_timer_ticks(calculate_timer_ticks(c->current_task));
}

void save_segments(const klib::shared_ptr<TaskDescriptor>& task)
{
    task->regs.seg.gs = read_msr(0xC0000102); // KernelGSBase
    task->regs.seg.fs = read_msr(0xC0000100); // FSBase
}

void restore_segments(const klib::shared_ptr<TaskDescriptor>& task)
{
    write_msr(0xC0000102, task->regs.seg.gs); // KernelGSBase
    write_msr(0xC0000100, task->regs.seg.fs); // FSBase
}

bool TaskDescriptor::atomic_try_unblock_by_page(u64 page)
{
    Auto_Lock_Scope scope_lock(sched_lock);

    if (status != PROCESS_BLOCKED)
        return false;

    if (page_blocked_by != page)
        return false;

    page_blocked_by = 0;
    unblock();
    return true;
}

bool TaskDescriptor::atomic_unblock_if_needed(const klib::shared_ptr<Generic_Port>& ptr)
{
    bool unblocked = false;
    Auto_Lock_Scope scope_lock(sched_lock);

    if (status != PROCESS_BLOCKED)
        return unblocked;

    if (page_blocked_by != 0)
        return unblocked;

    if (ptr and blocked_by.lock() == ptr) {
        unblocked = true;

        unblock();
    }
    return unblocked;
}

void TaskDescriptor::unblock()
{
    const auto self = weak_self.lock();

    auto * const p_queue = parent_queue;
    p_queue->atomic_erase(self);

    auto &local_cpu = *get_cpu_struct();

    if (cpu_affinity == 0 or cpu_affinity == local_cpu.cpu_id) {
        klib::shared_ptr<TaskDescriptor> current_task = get_cpu_struct()->current_task;

        if (current_task->priority > priority) {
            {
                Auto_Lock_Scope scope_l(current_task->sched_lock);

                switch_to();
            }

            push_ready(current_task);
        } else {
            push_ready(self);
        }
    } else {
        push_ready(self);

        // TODO: If other CPU is switching to a lower priority task, it might miss the newly pushed one and not execute it immediately
        // Not a big deal for now, but better approach is probably needed...
        auto &remote_cpu = *cpus[cpu_affinity-1].local_info;
        if (remote_cpu.current_task_priority > priority)
            remote_cpu.ipi_reschedule();
    }
}

// u64 TaskDescriptor::check_unblock_immediately(u64 reason, u64 extra)
// {
//     u64 mask = 0x01ULL << (reason - 1);

//     if ((this->unblock_mask & mask) and (this->block_extra == extra)) {
//         if (not this->messages.empty()) return reason;
//     }

//     return 0;
// }

void push_ready(const klib::shared_ptr<TaskDescriptor>& p)
{
    p->status = PROCESS_READY;

    const auto priority = p->priority;
    const priority_t priority_lim = global_sched_queues.size();

    if (priority < priority_lim) {
        const auto affinity = p->cpu_affinity;
        auto &sched_queues = affinity == 0 ? global_sched_queues : cpus[affinity-1].local_info->sched_queues;
        auto * const queue = &sched_queues[priority];

        p->parent_queue = queue;

        Auto_Lock_Scope lock(queue->lock);
        queue->push_back(p);
    } else {
        p->parent_queue = nullptr;
    }
}

void sched_periodic()
{
    // TODO: Replace with more sophisticated algorithm. Will definitely need to be redone once we have multi-cpu support

    CPU_Info* c = get_cpu_struct(); 

    klib::shared_ptr<TaskDescriptor> current = c->current_task;
    klib::shared_ptr<TaskDescriptor> next = c->atomic_pick_highest_priority(current->priority);

    if (next) {
        Auto_Lock_Scope_Double lock(current->sched_lock, next->sched_lock);

        current->status = Process_Status::PROCESS_READY;

        next->switch_to();

        push_ready(current);
    } else {
        start_timer_ticks(calculate_timer_ticks(current));
    }

    apic_eoi();
}

void start_scheduler()
{
    CPU_Info* s = get_cpu_struct();
    const klib::shared_ptr<TaskDescriptor>& t = s->current_task;

    start_timer_ticks(calculate_timer_ticks(t));
}

void reschedule()
{
    // TODO
}

u32 calculate_timer_ticks(const klib::shared_ptr<TaskDescriptor>& task)
{
    return assign_quantum_on_priority(task->priority)*ticks_per_1_ms;
}

klib::shared_ptr<TaskDescriptor> CPU_Info::atomic_pick_highest_priority(priority_t min)
{
    const priority_t max_priority = sched_queues.size() - 1;
    const priority_t to_priority = min > max_priority ? max_priority : min;

    for (priority_t i = 0; i <= to_priority; ++i) {
        klib::shared_ptr<TaskDescriptor> task;
        {
            auto& queue = sched_queues[i];

            Auto_Lock_Scope l(queue.lock);

            task = queue.pop_front();
        }

        if (task != klib::shared_ptr<TaskDescriptor>(nullptr)) return task;


        {
            auto& queue = global_sched_queues[i];

            Auto_Lock_Scope l(queue.lock);

            task = queue.pop_front();
        }

        if (task != klib::shared_ptr<TaskDescriptor>(nullptr)) return task;
    }

    return nullptr;
}

void find_new_process()
{
    CPU_Info& cpu_str = *get_cpu_struct();

    klib::shared_ptr<TaskDescriptor> next_task = cpu_str.atomic_pick_highest_priority();

    if (not next_task)
        next_task = cpu_str.idle_task;

    // t_print_bochs("Next task PID %i (%s). CPU %h\n", next_task->pid, next_task->name.c_str(), get_cpu_struct()->cpu_id);

    // Possible deadlock...?
    Auto_Lock_Scope(next_task->sched_lock);
    next_task->switch_to();
}

klib::shared_ptr<TaskDescriptor> sched_queue::pop_front() noexcept
{
    assert(lock.is_locked() and "Queue is not locked!");

    klib::shared_ptr<TaskDescriptor> ptr = first;

    if (first != klib::shared_ptr<TaskDescriptor>(nullptr))
        erase(ptr);

    return ptr;
}

void sched_queue::push_back(const klib::shared_ptr<TaskDescriptor>& desc) noexcept
{
    assert(lock.is_locked() and "Queue is not locked!");

    if (first == klib::shared_ptr<TaskDescriptor>(nullptr)) {
        first = desc;
        last = desc;
        desc->queue_prev = nullptr;
    } else {
        last->queue_next = desc;
        desc->queue_prev = last;
        last = desc;
    }

    desc->queue_next = nullptr;
    desc->parent_queue = this;
}

void sched_queue::push_front(const klib::shared_ptr<TaskDescriptor>& desc) noexcept
{
    assert(lock.is_locked() and "Queue is not locked!");


    if (first == klib::shared_ptr<TaskDescriptor>(nullptr)) {
        first = desc;
        last = desc;
        desc->queue_next = nullptr;
    } else {
        desc->queue_next = first;
        first->queue_prev = desc;
        first = desc;
    }

    desc->queue_prev = nullptr;
    desc->parent_queue = this;
}

void sched_queue::erase(const klib::shared_ptr<TaskDescriptor>& desc) noexcept
{
    assert(lock.is_locked() and "Queue is not locked!");

    if (desc->queue_prev) {
        desc->queue_prev->queue_next = desc->queue_next;
    } else {
        first = desc->queue_next;
    }

    if (desc->queue_next) {
        desc->queue_next->queue_prev = desc->queue_prev;
    } else {
        last = desc->queue_prev;
    }

    desc->queue_prev = nullptr;
    desc->queue_next = nullptr;
    desc->parent_queue = nullptr;
}

void sched_queue::atomic_erase(const klib::shared_ptr<TaskDescriptor>& desc) noexcept
{
    {
        Auto_Lock_Scope l(lock);
        if (desc->queue_prev) {
            desc->queue_prev->queue_next = desc->queue_next;
        } else {
            first = desc->queue_next;
        }

        if (desc->queue_next) {
            desc->queue_next->queue_prev = desc->queue_prev;
        } else {
            last = desc->queue_prev;
        }
    }

    desc->queue_prev = nullptr;
    desc->queue_next = nullptr;
    desc->parent_queue = nullptr;
}

quantum_t assign_quantum_on_priority(priority_t priority)
{
    static const quantum_t quantums[16] = {50, 50, 20, 20, 10, 10, 10, 5, 5, 5, 5, 5, 5, 5, 5, 5};

    if (priority < 16)
        return quantums[priority];

    return 100;
}

klib::shared_ptr<TaskDescriptor> CPU_Info::atomic_get_front_priority(priority_t priority)
{
    const priority_t priority_lim = sched_queues.size();

    if (priority < priority_lim) {
        sched_queue& queue = sched_queues[priority];

        Auto_Lock_Scope lock (queue.lock);

        return queue.pop_front();
    }

    return nullptr;
}

void CPU_Info::ipi_reschedule()
{
    // TODO
}

