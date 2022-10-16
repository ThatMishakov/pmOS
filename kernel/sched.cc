#include "sched.hh"
#include "asm.hh"
#include "paging.hh"
#include "types.hh"
#include "common/errors.h"
#include "linker.hh"

TaskDescriptor* current_task;

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
}

//TaskDescriptor* sched_pqueue::pop_front();

TaskDescriptor* sched_pqueue::get_first()
{
    return this->first;
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
}

ReturnStr<uint64_t> create_process()
{
    // BIG TODO: errors may produce **HUGE** memory leaks

    // Create the structure
    TaskDescriptor* n = new TaskDescriptor;

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

    PID pid_p = pid++;

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
    uint64_t stack_end = (uint64_t)&_free_after_kernel;
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