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

TaskDescriptor* idle_task;

sched_pqueue ready;

sched_pqueue blocked;
Spinlock blocked_s;

sched_pqueue uninit;
sched_pqueue dead;

sched_map s_map;

PID pid = 1;

void init_per_cpu()
{
    CPU_Info* c = new CPU_Info;
    write_msr(0xC0000101, (u64)c);
}

void init_scheduling()
{
    init_per_cpu();

    TaskDescriptor* current_task = new TaskDescriptor;
    get_cpu_struct()->current_task = current_task;
    current_task->page_table = getCR3();
    current_task->pid = pid++;
    s_map.insert({current_task->pid, current_task});

    init_idle();
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
    s_map.insert({n->pid, n});
    uninit.push_back(n);
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
        t_print("Error: failed to create the idle process!\n");
        return;
    }

    idle_task = s_map.at(i.val);

    // Init stack
    idle_task->init_stack();
    idle_task->regs.e.rip = (u64)&idle;
    uninit.erase(idle_task);
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
        find_new_process();
    }

    return {SUCCESS, 0};
}

void find_new_process()
{
    TaskDescriptor* next = nullptr;
    if (not ready.empty()) {
        next = ready.get_first();
        ready.pop_front();
    } else { // Nothing to execute. Idling...
        next = idle_task;
    }

    next->switch_to();
}

void TaskDescriptor::switch_to()
{
    // Change task
    get_cpu_struct()->next_task = this;
}

bool is_uninited(u64 pid)
{
    return s_map.at(pid)->status == PROCESS_UNINIT;
}

void TaskDescriptor::init_task()
{
    if (this->parrent != nullptr) this->parrent->erase(this);

    ready.push_back(this);
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
        ready.push_back(this);
    }
}

u64 TaskDescriptor::check_unblock_immediately()
{
    if (this->unblock_mask & MESSAGE_UNBLOCK_MASK) {
        if (not this->messages.empty()) return MESSAGE_S_NUM;
    }

    return 0;
}