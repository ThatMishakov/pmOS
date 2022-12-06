#include "sched.hh"

void sched_pqueue::atomic_auto_push_back(const klib::shared_ptr<TaskDescriptor>& task)
{
    Auto_Lock_Scope scope_lock(lock);

    klib::list<klib::weak_ptr<TaskDescriptor>>::iterator it = queue_list.push_back(task);

    // TODO make_unique exceptions

    klib::unique_ptr<generic_tqueue_iterator> ptr = klib::make_unique<iterator>(it, this);

    if (task->queue_iterator) {
        task->queue_iterator->atomic_erase_from_parrent();
    }

    task->queue_iterator = ptr;
}

void sched_pqueue::atomic_auto_push_front(const klib::shared_ptr<TaskDescriptor>& task)
{
    Auto_Lock_Scope scope_lock(lock);

    klib::list<klib::weak_ptr<TaskDescriptor>>::iterator it = queue_list.push_front(task);

    // TODO make_unique exceptions

    klib::unique_ptr<generic_tqueue_iterator> ptr = klib::make_unique<iterator>(it, this);

    if (task->queue_iterator) {
        task->queue_iterator->atomic_erase_from_parrent();
    }

    task->queue_iterator = ptr;
}

bool sched_pqueue::empty() const
{
    Auto_Lock_Scope scope_lock(lock);

    return queue_list.empty();
}
