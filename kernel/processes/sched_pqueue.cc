#include "sched.hh"

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
    lock.lock();
    bool is_empty = queue_list.empty();
    lock.unlock();
    return is_empty;
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