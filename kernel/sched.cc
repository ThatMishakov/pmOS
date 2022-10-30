#include "sched.hh"
#include "asm.hh"
#include "paging.hh"
#include "types.hh"
#include "common/errors.h"
#include "linker.hh"
#include "idle.hh"
#include "asm.hh"
#include "misc.hh"

TaskDescriptor* idle_task;

sched_pqueue ready;

sched_pqueue blocked;
Spinlock blocked_s;

sched_pqueue uninit;
sched_pqueue dead;

sched_map* s_map;

PID pid = 0;

void init_per_cpu()
{
    CPU_Info* c = new CPU_Info;
    write_msr(0xC0000101, (uint64_t)c);
}

void init_scheduling()
{
    init_per_cpu();
    s_map = new sched_map;

    TaskDescriptor* current_task = new TaskDescriptor;
    get_cpu_struct()->current_task = current_task;
    current_task->page_table = getCR3();
    current_task->pid = pid++;
    s_map->insert({current_task->pid, current_task});

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

ReturnStr<uint64_t> create_process(uint16_t ring)
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
        return {static_cast<kresult_t>(ERROR_NOT_SUPPORTED), (uint64_t)0};
        break;
    }

    // Create a new page table
    ReturnStr<uint64_t> k = get_new_pml4();
    if (k.result != SUCCESS) {
        delete n;
        return {k.result, 0};
    }
    n->page_table = k.val;

    // Assign a pid
    n->pid = assign_pid();

    // Init stack
    kresult_t status = n->init_stack();
    if (status != SUCCESS) return {status, 0};

    // Add to the map of processes and to uninit list
    s_map->insert({n->pid, n});
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

kresult_t TaskDescriptor::init_stack()
{
    // Switch to the new pml4
    uint64_t current_cr3 = getCR3();
    setCR3(this->page_table);

    kresult_t r;
    // Prealloc a page for the stack
    uint64_t stack_end = (uint64_t)&_free_to_use;
    uint64_t stack_page_start = stack_end - KB(4);

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
    return r;
}

void init_idle()
{
    ReturnStr<uint64_t> i = create_process(0);
    if (i.result != SUCCESS) {
        t_print("Error: failed to create the idle process!\n");
        return;
    }

    idle_task = s_map->at(i.result);
    idle_task->regs.e.rip = (uint64_t)&idle;
    uninit.erase(idle_task);
}

kresult_t TaskDescriptor::block()
{
    // Check status
    if (status == PROCESS_BLOCKED) return ERROR_ALREADY_BLOCKED;

    // Erase from queues
    if (this->parrent != nullptr) this->parrent->erase(this);

    // Add to blocked queue
    blocked_s.lock();
    blocked.push_back(this);
    blocked_s.unlock();

    // Task switch if it's a current process
    CPU_Info* cpu_str = get_cpu_struct();
    if (cpu_str->current_task == this) {
        cpu_str->current_task = nullptr;
        find_new_process();
    }

    return SUCCESS;
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
    t_print("Debug: switching to process with PID %h\n", pid);

    // Change task
    get_cpu_struct()->next_task = this;
}

bool is_uninited(uint64_t pid)
{
    return s_map->at(pid)->status == PROCESS_UNINIT;
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

    // Task switch if it's a current process
    CPU_Info* cpu_str = get_cpu_struct();
    if (cpu_str->current_task == p) {
        cpu_str->current_task = nullptr;
        find_new_process();
    }
}