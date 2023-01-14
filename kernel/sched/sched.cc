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

sched_queue blocked;
sched_queue uninit;
sched_queue dead;

Spinlock tasks_map_lock;
sched_map tasks_map;

PID pid = 1;

void init_scheduling()
{
    init_per_cpu();
    klib::shared_ptr<TaskDescriptor> current_task = klib::make_shared<TaskDescriptor>();
    get_cpu_struct()->current_task = current_task;
    current_task->page_table = getCR3();
    current_task->pid = pid++;    
    tasks_map.insert({current_task->pid, klib::move(current_task)});
}

DECLARE_LOCK(assign_pid);

PID assign_pid()
{
    LOCK(assign_pid)

    PID pid_p = pid++;

    UNLOCK(assign_pid)

   return pid_p; 
}

// ReturnStr<u64> block_task(const klib::shared_ptr<TaskDescriptor>& task, u64 mask)
// {
//     Auto_Lock_Scope scope_lock(task->sched_lock);

//     // Check status
//     if (task->status == PROCESS_BLOCKED) return {ERROR_ALREADY_BLOCKED, 0};

//     // Change mask if not null
//     if (mask != 0) task->unblock_mask = mask;

//     u64 imm = task->check_unblock_immediately();
//     if (imm != 0) return {SUCCESS, imm};

//     // Task switch if it's a current process
//     CPU_Info* cpu_str = get_cpu_struct();
//     if (cpu_str->current_task == task) {
//         task->quantum_ticks = apic_get_remaining_ticks();
//         task->next_status = Process_Status::PROCESS_BLOCKED;
//         find_new_process();
//     } else {
//         // Change status to blocked
//         task->status = PROCESS_BLOCKED;

//         blocked.atomic_auto_push_back(task);
//     }

//     return {SUCCESS, 0};
// }

// void find_new_process()
// {
//     CPU_Info* c = get_cpu_struct(); 
//     klib::pair<bool, klib::shared_ptr<TaskDescriptor>> t = c->sched.queues.atomic_get_pop_first();

//     if (not t.first) {
//         t.second = c->idle_task;
//     }


//     switch_to_task(t.second);
// }

// void switch_to_task(const klib::shared_ptr<TaskDescriptor>& task)
// {
//     // TODO: There is probably a race condition here

//     // Change task
//     task->status = Process_Status::PROCESS_RUNNING_IN_SYSTEM;
//     get_cpu_struct()->next_task = task;
//     start_timer_ticks(task->quantum_ticks);
// }

// void unblock_if_needed(const klib::shared_ptr<TaskDescriptor>& p, u64 reason)
// {
//     Auto_Lock_Scope scope_lock(p->sched_lock);

//     if (p->status != PROCESS_BLOCKED) return;
    
//     u64 mask = 0x01ULL << (reason - 1);

//     if (mask & p->unblock_mask) {
//         p->status = PROCESS_READY;
//         p->regs.scratch_r.rdi = reason;

//         push_ready(p);

//         klib::shared_ptr<TaskDescriptor> current = get_cpu_struct()->current_task;
//         if (current->quantum_ticks == 0)
//             evict(current);
//     }
// }

u64 TaskDescriptor::check_unblock_immediately()
{
    if (this->unblock_mask & MESSAGE_UNBLOCK_MASK) {
        if (not this->messages.empty()) return MESSAGE_S_NUM;
    }

    return 0;
}

// void push_ready(const klib::shared_ptr<TaskDescriptor>& p)
// {
//     get_cpu_struct()->sched.queues.temp_ready.atomic_auto_push_back(p);
// }

// void sched_periodic()
// {
//     // TODO: Replace with more sophisticated algorithm. Will definitely need to be redone once we have multi-cpu support

//     CPU_Info* c = get_cpu_struct(); 

//     klib::shared_ptr<TaskDescriptor> current = c->current_task;
//     klib::pair<bool, klib::shared_ptr<TaskDescriptor>> t = c->sched.queues.atomic_get_pop_first();

//     if (t.first) {
//         Ready_Queues::assign_quantum_on_priority(current.get());
//         current->next_status = Process_Status::PROCESS_READY;

//         switch_to_task(t.second);
//     } else
//         current->quantum_ticks = 0;

//     return;
// }

// void start_timer_ticks(u32 ticks)
// {
//     apic_one_shot_ticks(ticks);
//     get_cpu_struct()->sched.timer_val = ticks;
// }

// void start_timer(u32 ms)
// {
//     u32 ticks = ticks_per_1_ms*ms;
//     start_timer_ticks(ticks);
// }

// void start_scheduler()
// {
//     CPU_Info* s = get_cpu_struct();
//     const klib::shared_ptr<TaskDescriptor>& t = s->current_task;
//     Ready_Queues::assign_quantum_on_priority(t.get());
//     start_timer_ticks(t.get()->quantum_ticks);
// }

// void Ready_Queues::assign_quantum_on_priority(TaskDescriptor* t)
// {
//     t->quantum_ticks = Ready_Queues::quantums[t->priority] * ticks_per_1_ms;
// }

// void evict(const klib::shared_ptr<TaskDescriptor>& current_task)
// {
//     current_task->quantum_ticks = apic_get_remaining_ticks();
//     if (current_task->quantum_ticks == 0) Ready_Queues::assign_quantum_on_priority(current_task.get());

//     if (current_task->type == TaskDescriptor::Type::Idle) {
//         current_task->next_status = Process_Status::PROCESS_SPECIAL;
//     } else {
//         current_task->next_status = Process_Status::PROCESS_READY;
//     }
//     find_new_process();
// }

