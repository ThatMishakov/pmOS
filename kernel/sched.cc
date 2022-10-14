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

//void sched_pqueue::push_front(TaskDescriptor* d);

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
