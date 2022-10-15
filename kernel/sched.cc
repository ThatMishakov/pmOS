#include "sched.hh"
#include "asm.hh"
#include "paging.hh"
#include "types.hh"
#include "common/errors.h"

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
    TaskDescriptor* n = new TaskDescriptor;
    ReturnStr<uint64_t> k = get_new_pml4();
    if (k.result != SUCCESS) {
        delete n;
        return {k.result, 0};
    }
    n->page_table = k.val;
    n->pid = assign_pid();
    n->status = PROCESS_UNINIT;
    
    // TODO: Stack

    s_map->insert({n->pid, n});
    uninit.push_back(n);
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