#include "sched.hh"
#include "asm.hh"
#include "paging.hh"
#include "types.hh"
#include "common/errors.h"
#include "linker.hh"
#include "idle.hh"
#include "asm.hh"
#include "misc.hh"

TaskDescriptor* current_task;
TaskDescriptor* idle_task;

sched_pqueue ready;
sched_pqueue blocked;
sched_pqueue uninit;
sched_pqueue dead;

sched_map* s_map;

PID pid = 0;

void init_scheduling()
{
    s_map = new sched_map;
    current_task = new TaskDescriptor;
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
        n->regs.cs = R0_CODE_SEGMENT;
        n->regs.ss = R0_DATA_SEGMENT;
        break;
    case 3:
        n->regs.cs = R3_CODE_SEGMENT;
        n->regs.ss = R3_DATA_SEGMENT;
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
    kresult_t status = init_stack(n);
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

    PID pid_p = ++pid;

    UNLOCK(assign_pid)

   return pid_p; 
}

kresult_t init_stack(TaskDescriptor* process)
{
    // Switch to the new pml4
    uint64_t current_cr3 = getCR3();
    setCR3(process->page_table);

    kresult_t r;
    // Prealloc a page for the stack
    uint64_t stack_end = (uint64_t)&_free_to_use;
    uint64_t stack_page_start = stack_end - KB(4);

    Page_Table_Argumments arg;
    arg.writeable = 1;
    arg.execution_disabled = 1;
    arg.global = 0;
    arg.user_access = 1;
    r = alloc_page_lazy(stack_page_start, arg);
    if (r != SUCCESS) goto fail;

    // Set new rsp
    process->regs.rsp = stack_end;
    process->regs.rbp = stack_end;
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
    idle_task->regs.rip = (uint64_t)&idle;
    uninit.erase(idle_task);
}

kresult_t block_process(TaskDescriptor* p)
{
    // Check status
    if (p->status == PROCESS_BLOCKED) return ERROR_ALREADY_BLOCKED;

    // Erase from queues
    if (p->parrent != nullptr) p->parrent->erase(p);

    // Add to blocked queue
    blocked.push_back(p);

    // Task switch if it's a current process
    if (current_task == p) {
        current_task = nullptr;
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

    switch_process(next);
}

void switch_process(TaskDescriptor* p)
{
    // Load CR3
    setCR3(p->page_table);

    // Set segment registers
    set_segment_regs(p->regs.ss);

    // Change task
    current_task = p;
}

bool is_uninited(uint64_t pid)
{
    return s_map->at(pid)->status == PROCESS_UNINIT;
}

void init_task(TaskDescriptor* d)
{
    if (d->parrent != nullptr) d->parrent->erase(d);

    ready.push_back(d);
}

void kill(TaskDescriptor* p)
{
    // Erase from queues
    if (p->parrent != nullptr) p->parrent->erase(p);

    // Add to blocked queue
    dead.push_back(p);

    // Task switch if it's a current process
    if (current_task == p) {
        current_task = nullptr;
        find_new_process();
    }
}