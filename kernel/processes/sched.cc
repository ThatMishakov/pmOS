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
#include <cpus/cpu_init.hh>

sched_pqueue blocked;
Spinlock blocked_s;

Spinlock uninit_lock;
sched_pqueue uninit;

sched_pqueue dead;

Spinlock tasks_map_lock;
sched_map tasks_map;

PID pid = 1;

void init_scheduling()
{
    init_per_cpu();

    TaskDescriptor* current_task = new TaskDescriptor;
    get_cpu_struct()->current_task = current_task;
    current_task->page_table = getCR3();
    current_task->pid = pid++;
    tasks_map.insert({current_task->pid, current_task});
}

void sched_pqueue::push_back(TaskDescriptor* d)
{
    if (this->last != nullptr) {
        d->q_prev = this->last;
        this->last->q_next = d;
        d->q_next = nullptr;
        this->last = d;
    } else {
        this->first = d;
        this->last = d;
        d->q_prev = nullptr;
        d->q_next = nullptr;
    }
    d->parrent = this;
}

TaskDescriptor* sched_pqueue::pop_front()
{
    TaskDescriptor* t = this->get_first();
    this->erase(t);
    return t;
}

TaskDescriptor* sched_pqueue::get_first()
{
    return this->first;
}

bool sched_pqueue::empty() const
{
    return this->first == nullptr;
}

void sched_pqueue::erase(TaskDescriptor* t)
{
    if (this->first == t) {
        this->first = t->q_next;
    } else {
        t->q_prev->q_next = t->q_next;
    }

    if (this->last == t) {
        this->last = t->q_prev;
    } else {
        t->q_next->q_prev = t->q_prev;
    }
    t->parrent = nullptr;
}

ReturnStr<u64> create_process(u16 ring)
{
    // BIG TODO: errors may produce **HUGE** memory leaks

    // Create the structure
    TaskDescriptor* n = new TaskDescriptor;

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
        return {static_cast<kresult_t>(ERROR_NOT_SUPPORTED), (u64)0};
        break;
    }

    // Create a new page table
    ReturnStr<u64> k = get_new_pml4();
    if (k.result != SUCCESS) {
        delete n;
        return {k.result, 0};
    }
    n->page_table = k.val;

    // Assign a pid
    n->pid = assign_pid();

    // Add to the map of processes and to uninit list
    tasks_map_lock.lock();
    tasks_map.insert({n->pid, n});
    tasks_map_lock.unlock();
    uninit_lock.lock();
    uninit.push_back(n);
    uninit_lock.unlock();
    n->status = PROCESS_UNINIT;
    
    // Success!
    return {SUCCESS, n->pid};
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
    ReturnStr<u64> i = create_process(0);
    if (i.result != SUCCESS) {
        t_print_bochs("Error: failed to create the idle process!\n");
        return;
    }

    CPU_Info* cpu_str = get_cpu_struct();
    tasks_map_lock.lock();
    cpu_str->idle_task = tasks_map.at(i.val);
    tasks_map_lock.unlock();
    TaskDescriptor* idle_task = cpu_str->idle_task;

    // Init stack
    idle_task->init_stack();
    idle_task->regs.e.rip = (u64)&idle;
    idle_task->type = TaskDescriptor::Type::Idle;
    idle_task->priority = TaskDescriptor::background_priority;
    Ready_Queues::assign_quantum_on_priority(idle_task);
    uninit_lock.lock();
    uninit.erase(idle_task);
    uninit_lock.unlock();
}

ReturnStr<u64> TaskDescriptor::block(u64 mask)
{
    // Check status
    if (status == PROCESS_BLOCKED) return {ERROR_ALREADY_BLOCKED, 0};

    // Change mask if not null
    if (mask != 0) this->unblock_mask = mask;

    u64 imm = check_unblock_immediately();
    if (imm != 0) return {SUCCESS, imm};

    // Erase from queues
    if (this->parrent != nullptr) this->parrent->erase(this);

    // Change status to blocked
    status = PROCESS_BLOCKED;

    // Add to blocked queue
    blocked_s.lock();
    blocked.push_back(this);
    blocked_s.unlock();

    // Task switch if it's a current process
    CPU_Info* cpu_str = get_cpu_struct();
    if (cpu_str->current_task == this) {
        cpu_str->current_task->quantum_ticks = apic_get_remaining_ticks();
        find_new_process();
    }

    return {SUCCESS, 0};
}

void find_new_process()
{
    TaskDescriptor* next = nullptr;
    if (not get_cpu_struct()->sched.queues.temp_ready.empty()) {
        next = get_cpu_struct()->sched.queues.temp_ready.get_first();
        get_cpu_struct()->sched.queues.temp_ready.pop_front();
    } else { // Nothing to execute. Idling...
        next = get_cpu_struct()->idle_task;
    }

    next->switch_to();
}

void TaskDescriptor::switch_to()
{
    // Change task
    get_cpu_struct()->next_task = this;
    start_timer_ticks(this->quantum_ticks);
}

bool is_uninited(u64 pid)
{
    tasks_map_lock.lock();
    bool is_uninited_b = tasks_map.at(pid)->status == PROCESS_UNINIT;
    tasks_map_lock.unlock();
    return is_uninited_b;
}

void TaskDescriptor::init_task()
{
    Ready_Queues::assign_quantum_on_priority(this);
    push_ready(this);
}

void kill(TaskDescriptor* p)
{
    // Erase from queues
    if (p->parrent != nullptr) p->parrent->erase(p);

    // Add to dead queue
    dead.push_back(p);

    // Release user pages
    u64 ptable = p->page_table;
    free_user_pages(ptable, p->pid);

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

void TaskDescriptor::unblock_if_needed(u64 reason)
{
    if (this->status != PROCESS_BLOCKED) return;
    
    u64 mask = 0x01ULL << (reason - 1);

    if (mask & this->unblock_mask) {
        this->parrent->erase(this);
        this->status = PROCESS_READY;
        this->regs.scratch_r.rdi = reason;
        push_ready(this);
        TaskDescriptor* current = get_cpu_struct()->current_task;
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

void push_ready(TaskDescriptor* t)
{
    if (t->parrent != nullptr) t->parrent->erase(t);

    get_cpu_struct()->sched.queues.temp_ready.push_back(t);
}

void sched_periodic()
{
    // TODO: Replace with more sophisticated algorithm. Will definitely need to be redone once we have multi-cpu support

    TaskDescriptor* current = get_cpu_struct()->current_task;

    if (not get_cpu_struct()->sched.queues.temp_ready.empty()) {
        Ready_Queues::assign_quantum_on_priority(current);
        push_ready(current);
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
    TaskDescriptor* t = s->current_task;
    Ready_Queues::assign_quantum_on_priority(t);
    start_timer_ticks(t->quantum_ticks);
}

void Ready_Queues::assign_quantum_on_priority(TaskDescriptor* t)
{
    t->quantum_ticks = Ready_Queues::quantums[t->priority] * ticks_per_1_ms;
}

void evict(TaskDescriptor* current_task)
{
    current_task->quantum_ticks = apic_get_remaining_ticks();
    if (current_task->quantum_ticks == 0) Ready_Queues::assign_quantum_on_priority(current_task);

    if (current_task->type != TaskDescriptor::Type::Idle) {
        push_ready(current_task);
    }
    find_new_process();
}