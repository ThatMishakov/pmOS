#include "sched.hh"
#include <asm.hh>
#include <memory/paging.hh>
#include <types.hh>
#include <kernel/errors.h>
#include <linker.hh>
#include "idle.hh"
#include <asm.hh>
#include <kernel/com.h>
#include <kernel/block.h>
#include <misc.hh>
#include <interrupts/apic.hh>
#include <cpus/cpus.hh>
#include <lib/memory.hh>

sched_pqueue blocked;
sched_pqueue uninit;
sched_pqueue dead;

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

ReturnStr<klib::shared_ptr<TaskDescriptor>> create_process(u16 ring)
{
    // Create the structure
    klib::shared_ptr<TaskDescriptor> n = klib::make_shared<TaskDescriptor>();
    // Assign cs and ss
    switch (ring)
    {
    case 0:
        n->regs.e.cs = R0_CODE_SEGMENT;
        n->regs.e.ss = R0_DATA_SEGMENT;
        break;
    case 3:
        n->regs.e.cs = R3_CODE_SEGMENT;
        n->regs.e.ss = R3_DATA_SEGMENT;
        break;
    default:
        return {static_cast<kresult_t>(ERROR_NOT_SUPPORTED), klib::shared_ptr<TaskDescriptor>()};
        break;
    }

    // Create a new page table
    ReturnStr<u64> k = get_new_pml4();
    if (k.result != SUCCESS) {
        return {k.result, klib::shared_ptr<TaskDescriptor>()};
    }
    n->page_table = k.val;

    // Assign a pid
    n->pid = assign_pid();
    n->status = PROCESS_UNINIT;

    // Add to the map of processes and to uninit list
    tasks_map_lock.lock();
    tasks_map.insert({n->pid, n});
    tasks_map_lock.unlock();

    uninit.atomic_auto_push_back(n);    

    // Success!
    return {SUCCESS, n};
}

DECLARE_LOCK(assign_pid);

PID assign_pid()
{
    LOCK(assign_pid)

    PID pid_p = pid++;

    UNLOCK(assign_pid)

   return pid_p; 
}

ReturnStr<u64> TaskDescriptor::init_stack()
{
    // Switch to the new pml4
    u64 current_cr3 = getCR3();
    setCR3(this->page_table);

    kresult_t r;
    // Prealloc a page for the stack
    u64 stack_end = (u64)KERNEL_ADDR_SPACE; //&_free_to_use;
    u64 stack_page_start = stack_end - KB(4);

    Page_Table_Argumments arg;
    arg.writeable = 1;
    arg.execution_disabled = 1;
    arg.global = 0;
    arg.user_access = 1;
    r = alloc_page_lazy(stack_page_start, arg);  // TODO: This crashes real machines

    if (r != SUCCESS) goto fail;

    // Set new rsp
    this->regs.e.rsp = stack_end;
fail:
    // Load old page table back
    setCR3(current_cr3);
    return {r, this->regs.e.rsp};
}

void init_idle()
{
    ReturnStr<klib::shared_ptr<TaskDescriptor>> i = create_process(0);
    if (i.result != SUCCESS) {
        t_print_bochs("Error: failed to create the idle process!\n");
        return;
    }

    CPU_Info* cpu_str = get_cpu_struct();
    cpu_str->idle_task = i.val;

    // Init stack
    i.val->init_stack();
    i.val->regs.e.rip = (u64)&idle;
    i.val->type = TaskDescriptor::Type::Idle;
    i.val->priority = TaskDescriptor::background_priority;
    Ready_Queues::assign_quantum_on_priority(i.val.get());

    i.val->queue_iterator->atomic_erase_from_parrent();
}

ReturnStr<u64> block_task(const klib::shared_ptr<TaskDescriptor>& task, u64 mask)
{
    Auto_Lock_Scope scope_lock(task->sched_lock);

    // Check status
    if (task->status == PROCESS_BLOCKED) return {ERROR_ALREADY_BLOCKED, 0};

    // Change mask if not null
    if (mask != 0) task->unblock_mask = mask;

    u64 imm = task->check_unblock_immediately();
    if (imm != 0) return {SUCCESS, imm};

    // Task switch if it's a current process
    CPU_Info* cpu_str = get_cpu_struct();
    if (cpu_str->current_task == task) {
        task->quantum_ticks = apic_get_remaining_ticks();
        task->next_status = Process_Status::PROCESS_BLOCKED;
        find_new_process();
    } else {
        // Change status to blocked
        task->status = PROCESS_BLOCKED;

        blocked.atomic_auto_push_back(task);
    }

    return {SUCCESS, 0};
}

void find_new_process()
{
    CPU_Info* c = get_cpu_struct(); 
    klib::pair<bool, klib::shared_ptr<TaskDescriptor>> t = c->sched.queues.atomic_get_pop_first();

    if (not t.first) {
        t.second = c->idle_task;
    }


    switch_to_task(t.second);
}

void switch_to_task(const klib::shared_ptr<TaskDescriptor>& task)
{
    // TODO: There is probably a race condition here

    // Change task
    task->status = Process_Status::PROCESS_RUNNING_IN_SYSTEM;
    get_cpu_struct()->next_task = task;
    start_timer_ticks(task->quantum_ticks);
}

bool is_uninited(const klib::shared_ptr<const TaskDescriptor>& task)
{
    return task->status == PROCESS_UNINIT;
}

void init_task(const klib::shared_ptr<TaskDescriptor>& task)
{
    Ready_Queues::assign_quantum_on_priority(task.get());
    push_ready(task);
}

void kill(const klib::shared_ptr<TaskDescriptor>& p)
{
    Auto_Lock_Scope scope_lock(p->sched_lock);

    // Add to dead queue
    dead.atomic_auto_push_back(p);

    // Release user pages
    u64 ptable = p->page_table;
    free_user_pages(ptable, p->pid);

    /* --- TODO: This can be simplified now that I use shared_ptr ---*/

    // Task switch if it's a current process
    CPU_Info* cpu_str = get_cpu_struct();
    if (cpu_str->current_task == p) {
        cpu_str->current_task = nullptr;
        cpu_str->release_old_cr3 = 1;
        find_new_process();
    } else {
        release_cr3(ptable);
    }
}

void unblock_if_needed(const klib::shared_ptr<TaskDescriptor>& p, u64 reason)
{
    Auto_Lock_Scope scope_lock(p->sched_lock);

    if (p->status != PROCESS_BLOCKED) return;
    
    u64 mask = 0x01ULL << (reason - 1);

    if (mask & p->unblock_mask) {
        p->status = PROCESS_READY;
        p->regs.scratch_r.rdi = reason;

        push_ready(p);

        klib::shared_ptr<TaskDescriptor> current = get_cpu_struct()->current_task;
        if (current->quantum_ticks == 0)
            evict(current);
    }
}

u64 TaskDescriptor::check_unblock_immediately()
{
    if (this->unblock_mask & MESSAGE_UNBLOCK_MASK) {
        if (not this->messages.empty()) return MESSAGE_S_NUM;
    }

    return 0;
}

void push_ready(const klib::shared_ptr<TaskDescriptor>& p)
{
    get_cpu_struct()->sched.queues.temp_ready.atomic_auto_push_back(p);
}

void sched_periodic()
{
    // TODO: Replace with more sophisticated algorithm. Will definitely need to be redone once we have multi-cpu support

    const klib::shared_ptr<TaskDescriptor>& current = get_cpu_struct()->current_task;

    if (not get_cpu_struct()->sched.queues.temp_ready.empty()) {
        Ready_Queues::assign_quantum_on_priority(current.get());
        current->next_status = Process_Status::PROCESS_READY;
        find_new_process();
    } else
        current->quantum_ticks = 0;

    return;
}

void start_timer_ticks(u32 ticks)
{
    apic_one_shot_ticks(ticks);
    get_cpu_struct()->sched.timer_val = ticks;
}

void start_timer(u32 ms)
{
    u32 ticks = ticks_per_1_ms*ms;
    start_timer_ticks(ticks);
}

void start_scheduler()
{
    CPU_Info* s = get_cpu_struct();
    const klib::shared_ptr<TaskDescriptor>& t = s->current_task;
    Ready_Queues::assign_quantum_on_priority(t.get());
    start_timer_ticks(t.get()->quantum_ticks);
}

void Ready_Queues::assign_quantum_on_priority(TaskDescriptor* t)
{
    t->quantum_ticks = Ready_Queues::quantums[t->priority] * ticks_per_1_ms;
}

void evict(const klib::shared_ptr<TaskDescriptor>& current_task)
{
    current_task->quantum_ticks = apic_get_remaining_ticks();
    if (current_task->quantum_ticks == 0) Ready_Queues::assign_quantum_on_priority(current_task.get());

    if (current_task->type == TaskDescriptor::Type::Idle) {
        current_task->next_status = Process_Status::PROCESS_SPECIAL;
    } else {
        current_task->next_status = Process_Status::PROCESS_READY;
    }
    find_new_process();
}

klib::pair<bool, klib::shared_ptr<TaskDescriptor>> Ready_Queues::atomic_get_pop_first()
{
    klib::pair<bool, klib::weak_ptr<TaskDescriptor>> t;

    do {
        t = temp_ready.atomic_pop_front();

        if (t.first) {
            klib::shared_ptr<TaskDescriptor> task = t.second.lock();

            if (task) {
                task->queue_iterator = nullptr;
                return {true, task};
            }
        }
    } while (t.first);

    return {false, nullptr};
}

