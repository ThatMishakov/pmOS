#include "sched.hh"
#include "asm.hh"

TaskDescriptor* current_task;

sched_pqueue ready;
sched_pqueue blocked;

void init_scheduling()
{
    current_task = new TaskDescriptor;
    current_task->page_table = getCR3();
}

void sched_pqueue::push_back(TaskDescriptor* d)
{
    if (this->last != nullptr) {
        this->first = d;
        this->last = d;
        d->q_next = nullptr;
    } else {
        this->last->q_next = d;
        d->q_next = nullptr;
        this->last = d;
    }
}

void sched_pqueue::push_front(TaskDescriptor* d)
{
    if (this->last == nullptr) this->last = d;
    d->q_next = this->first;
    this->first = d;
}

TaskDescriptor* sched_pqueue::pop_front()
{
    TaskDescriptor* q = this->first;
    if (this->first != nullptr) this->first = this->first->q_next;
    if (this->last == q) this->last = nullptr;
    q->q_next = nullptr;
    return q;
}

TaskDescriptor* sched_pqueue::get_first()
{
    return this->first;
}
