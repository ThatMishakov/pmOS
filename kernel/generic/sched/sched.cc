#include "sched.hh"
#include <memory/paging.hh>
#include <types.hh>
#include <kernel/errors.h>
#include <linker.hh>
#include "processes/idle.hh"
#include <kernel/com.h>
#include <kernel/block.h>
#include <misc.hh>
#include <lib/memory.hh>
#include "timers.hh"
#include <stdlib.h>
#include <assert.h>

sched_queue blocked;
sched_queue uninit;

Spinlock tasks_map_lock;
sched_map tasks_map;

klib::vector<CPU_Info*> cpus;

PID pid = 1;

PID assign_pid()
{
   PID pid_p = __atomic_fetch_add(&pid, 1, __ATOMIC_SEQ_CST);
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


void TaskDescriptor::atomic_block_by_page(u64 page, sched_queue *blocked_ptr)
{
    assert(status != PROCESS_BLOCKED && "task cannot be blocked twice");

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
        Auto_Lock_Scope scope_l(blocked_ptr->lock);
        blocked_ptr->push_back(self);
    }
}

void TaskDescriptor::switch_to()
{
    CPU_Info *c = get_cpu_struct();
    if (c->current_task->page_table != page_table) {
        c->current_task->page_table->atomic_active_sum(-1);
        page_table->atomic_active_sum(1);
        page_table->apply();
    }

    c->current_task->before_task_switch();

    // Switch task task
    status = Process_Status::PROCESS_RUNNING;
    c->current_task_priority = priority;
    c->current_task = weak_self.lock();

    this->after_task_switch();

    start_timer(assign_quantum_on_priority(priority));
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

void TaskDescriptor::atomic_erase_from_queue(sched_queue *q) noexcept
{
    Auto_Lock_Scope l(sched_lock);

    if (parent_queue != q) {
        [[unlikely]];
        return;
    }
    
    unblock();
}

void TaskDescriptor::unblock() noexcept
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
        auto remote_cpu = cpus[cpu_affinity-1];
        if (remote_cpu->current_task_priority > priority)
            remote_cpu->ipi_reschedule();
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
        auto &sched_queues = affinity == 0 ? global_sched_queues : cpus[affinity-1]->sched_queues;
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
        start_timer(assign_quantum_on_priority(current->priority));
    }
}

void start_scheduler()
{
    CPU_Info* s = get_cpu_struct();
    const klib::shared_ptr<TaskDescriptor>& t = s->current_task;

    start_timer(assign_quantum_on_priority(t->priority));
}

void reschedule()
{
    auto * const cpu_str = get_cpu_struct();
    const auto current_priority = cpu_str->current_task->priority;

    auto const new_task = cpu_str->atomic_pick_highest_priority(current_priority - 1);
    if (new_task) {
        auto const current_task = cpu_str->current_task;

        // It might be fine to lock the locks separately
        Auto_Lock_Scope_Double l(current_task->sched_lock, new_task->sched_lock);

        new_task->switch_to();
        push_ready(current_task);
    }
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

