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

#include "sched.hh"

#include "processes/idle.hh"
#include "timers.hh"

#include <assert.h>
#include <errno.h>
#include <kernel/block.h>
#include <kernel/com.h>
#include <lib/memory.hh>
#include <linker.hh>
#include <memory/paging.hh>
#include <misc.hh>
#include <pmos/ipc.h>
#include <stdlib.h>
#include <types.hh>

sched_queue blocked;
sched_queue uninit;
sched_queue paused;

Spinlock tasks_map_lock;
sched_map tasks_map;

RCU paging_rcu;
RCU heap_rcu;

klib::vector<CPU_Info *> cpus;

size_t get_cpu_count() noexcept { return cpus.size(); }

ReturnStr<u64> block_current_task(Generic_Port *ptr)
{
    // TODO: This function has a strange return value
    TaskDescriptor *task = get_cpu_struct()->current_task;

    // t_print_bochs("Blocking %i (%s) by port\n", task->pid, task->name.c_str());

    Auto_Lock_Scope scope_lock(task->sched_lock);
    // If the task is dying, don't block it
    if (task->status == TaskStatus::TASK_DYING)
        return {0, 0};

    task->status       = TaskStatus::TASK_BLOCKED;
    task->blocked_by   = ptr;
    task->parent_queue = &blocked;

    {
        Auto_Lock_Scope scope_l(blocked.lock);
        blocked.push_back(task);
    }

    // Task switch
    find_new_process();

    return {0, 0};
}

void TaskDescriptor::atomic_block_by_page(u64 page, sched_queue *blocked_ptr)
{
    assert(status != TaskStatus::TASK_BLOCKED && "task cannot be blocked twice");

    // t_print_bochs("Blocking %i (%s) by page. CPU %i\n", this->pid, this->name.c_str(),
    // get_cpu_struct()->cpu_id);

    Auto_Lock_Scope scope_lock(sched_lock);
    // If the task is dying, don't actually block it
    if (status == TaskStatus::TASK_DYING)
        return;

    status          = TaskStatus::TASK_BLOCKED;
    page_blocked_by = page;

    if (get_cpu_struct()->current_task == this) {
        find_new_process();
    } else if (parent_queue) {
        Auto_Lock_Scope scope_l(parent_queue->lock);
        parent_queue->erase(this);
    }

    {
        Auto_Lock_Scope scope_l(blocked_ptr->lock);
        blocked_ptr->push_back(this);
    }
}

void service_timer_ports()
{
    auto c            = get_cpu_struct();
    auto current_time = get_current_time_ticks();
    Auto_Lock_Scope l(c->timer_lock);

    for (auto t = c->timer_queue.begin(); t != c->timer_queue.end() and t->first < current_time;) {
        auto port = Port::atomic_get_port(t->second);
        if (port) {
            IPC_Timer_Reply r = {
                IPC_Timer_Reply_NUM,
            };
            port->atomic_send_from_system(reinterpret_cast<char *>(&r), sizeof(r));
        }

        auto o = t++;
        c->timer_queue.erase(o);
    }
}

void CPU_Info::atomic_timer_queue_push(u64 fire_on_core_ticks, Port *port)
{
    Auto_Lock_Scope l(timer_lock);

    timer_queue[fire_on_core_ticks] = port->portno;
}

extern klib::shared_ptr<Arch_Page_Table> idle_page_table;

void TaskDescriptor::switch_to()
{
    CPU_Info *c = get_cpu_struct();
    assert(cpu_affinity == 0 or (cpu_affinity - 1) == c->cpu_id);

    if (c->current_task->page_table != page_table) {
        page_table->atomic_active_sum(1);
        c->current_task->page_table->atomic_active_sum(-1);

        auto new_page_table = page_table ? page_table : idle_page_table;

        new_page_table->apply();
        c->paging_rcu_cpu.quiet(paging_rcu, c->cpu_id);
    }

    c->current_task->before_task_switch();

    // Switch task
    if (status != TaskStatus::TASK_DYING)
        // If the task is dying, don't change its status and let the scheduler handle it when
        // returning from the kernel
        status = TaskStatus::TASK_RUNNING;

    c->current_task_priority = priority;
    c->current_task          = this;

    this->after_task_switch();

    start_timer(assign_quantum_on_priority(priority));
}

bool TaskDescriptor::atomic_try_unblock_by_page(u64 page)
{
    Auto_Lock_Scope scope_lock(sched_lock);

    if (status != TaskStatus::TASK_BLOCKED)
        return false;

    if (page_blocked_by != page)
        return false;

    page_blocked_by = 0;
    unblock();
    return true;
}

bool TaskDescriptor::atomic_unblock_if_needed(Generic_Port *ptr)
{
    bool unblocked = false;
    Auto_Lock_Scope scope_lock(sched_lock);

    if (status != TaskStatus::TASK_BLOCKED)
        return unblocked;

    if (page_blocked_by != 0)
        return unblocked;

    if (ptr and blocked_by == ptr) {
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

void TaskDescriptor::atomic_try_unblock()
{
    Auto_Lock_Scope scope_lock(sched_lock);

    if (status != TaskStatus::TASK_BLOCKED)
        return;

    unblock();
}

void TaskDescriptor::unblock() noexcept
{
    auto *const p_queue = parent_queue;
    p_queue->atomic_erase(this);

    auto &local_cpu = *get_cpu_struct();

    if (cpu_affinity == 0 or ((cpu_affinity - 1) == local_cpu.cpu_id)) {
        TaskDescriptor *current_task = get_cpu_struct()->current_task;

        if (current_task->priority > priority) {
            if (status == TaskStatus::TASK_DYING) {
                cleanup();
                return;
            }

            {
                Auto_Lock_Scope scope_l(current_task->sched_lock);
                switch_to();
            }

            push_ready(current_task);
        } else {
            push_ready(this);
        }
    } else {
        push_ready(this);

        // TODO: If other CPU is switching to a lower priority task, it might miss the newly pushed
        // one and not execute it immediately. Not a big deal for now, but better approach is
        // probably needed...
        auto remote_cpu = cpus[cpu_affinity - 1];
        assert(remote_cpu->cpu_id == cpu_affinity - 1);
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

void push_ready(TaskDescriptor *p)
{
    if (p->status != TaskStatus::TASK_DYING)
        // Carry dying status, set to ready otherwise
        p->status = TaskStatus::TASK_READY;

    const auto priority           = p->priority;
    const priority_t priority_lim = global_sched_queues.size();

    if (priority < priority_lim) {
        const auto affinity = p->cpu_affinity;
        auto &sched_queues = affinity == 0 ? global_sched_queues : cpus[affinity - 1]->sched_queues;
        auto *const queue  = &sched_queues[priority];

        p->parent_queue = queue;

        Auto_Lock_Scope lock(queue->lock);
        queue->push_back(p);
    } else {
        p->parent_queue = nullptr;
    }
}

void sched_periodic()
{
    CPU_Info *c = get_cpu_struct();

    // Quiet RCU
    // Since this function is called from the timer interrupt, no context is held here
    c->heap_rcu_cpu.quiet(heap_rcu, c->cpu_id);

    // TODO: Replace with more sophisticated algorithm. Will definitely need to be redone once we
    // have multi-cpu support

    TaskDescriptor *current = c->current_task;
    TaskDescriptor *next    = c->atomic_pick_highest_priority(current->priority);

    if (next) {
        Auto_Lock_Scope_Double lock(current->sched_lock, next->sched_lock);

        next->switch_to();

        push_ready(current);
    } else {
        start_timer(assign_quantum_on_priority(current->priority));
    }

    while (c->current_task->status == TaskStatus::TASK_DYING) {
        c->current_task->cleanup();

        find_new_process();
    }

    // Funny bug, that took 1 monts to find and fix: if this is done between when the copy of the
    // current task is made and the task switch, the current task might disappear, since
    // `service_timer_ports()` can reschedule and current task will no longer be running
    service_timer_ports();
}

void start_scheduler()
{
    CPU_Info *s       = get_cpu_struct();
    TaskDescriptor *t = s->current_task;

    start_timer(assign_quantum_on_priority(t->priority));
}

void reschedule()
{
    auto *const cpu_str         = get_cpu_struct();
    const auto current_priority = cpu_str->current_task->priority;

    auto const new_task = cpu_str->atomic_pick_highest_priority(current_priority - 1);
    if (new_task) {
        auto const current_task = cpu_str->current_task;

        // It might be fine to lock the locks separately
        Auto_Lock_Scope_Double l(current_task->sched_lock, new_task->sched_lock);

        new_task->switch_to();
        push_ready(current_task);
    }

    while (cpu_str->current_task->status == TaskStatus::TASK_DYING) {
        auto t = cpu_str->current_task;
        t->cleanup();

        find_new_process();
    }

    while (cpu_str->current_task->sched_pending_mask & TaskDescriptor::SCHED_PENDING_PAUSE) {
        auto current_task = cpu_str->current_task;

        {
            Auto_Lock_Scope l(current_task->sched_lock);
            if (current_task->status != TaskStatus::TASK_DYING) {
                current_task->status = TaskStatus::TASK_PAUSED;

                current_task->parent_queue = &paused;
                {
                    Auto_Lock_Scope l(paused.lock);
                    paused.push_back(current_task);
                }
                find_new_process();
            }

            bool cont = true;
            while (cont) {
                TaskDescriptor *t;
                {
                    // TODO: This is potentially a race condition
                    Auto_Lock_Scope scope_lock(current_task->waiting_to_pause.lock);
                    t = current_task->waiting_to_pause.front();
                }
                if (!t)
                    cont = false;
                else {
                    t->atomic_try_unblock();
                }
            }
        }
    }
}

TaskDescriptor *CPU_Info::atomic_pick_highest_priority(priority_t min)
{
    const priority_t max_priority = sched_queues.size() - 1;
    const priority_t to_priority  = min > max_priority ? max_priority : min;

    for (priority_t i = 0; i <= to_priority; ++i) {
        TaskDescriptor *task;
        {
            auto &queue = sched_queues[i];

            Auto_Lock_Scope l(queue.lock);

            task = queue.pop_front();
        }

        if (task)
            return task;

        {
            auto &queue = global_sched_queues[i];

            Auto_Lock_Scope l(queue.lock);

            task = queue.pop_front();
        }

        if (task)
            return task;
    }

    return nullptr;
}

void find_new_process()
{
    CPU_Info &cpu_str = *get_cpu_struct();

    TaskDescriptor *next_task = cpu_str.atomic_pick_highest_priority();

    while (next_task and next_task->status == TaskStatus::TASK_DYING) {
        next_task->cleanup();
        next_task = cpu_str.atomic_pick_highest_priority();
    }

    if (not next_task)
        next_task = cpu_str.idle_task;

    // t_print_bochs("Next task PID %i (%s). CPU %h\n", next_task->pid, next_task->name.c_str(),
    // get_cpu_struct()->cpu_id);

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

TaskDescriptor *CPU_Info::atomic_get_front_priority(priority_t priority)
{
    const priority_t priority_lim = sched_queues.size();

    if (priority < priority_lim) {
        sched_queue &queue = sched_queues[priority];

        Auto_Lock_Scope lock(queue.lock);

        return queue.pop_front();
    }

    return nullptr;
}
