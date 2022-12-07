#include "sched.hh"

void sched_pqueue::atomic_auto_push_back(const klib::shared_ptr<TaskDescriptor>& task)
{
    Auto_Lock_Scope scope_lock(lock);

    klib::unique_ptr<iterator> ptr = klib::make_unique<iterator>(klib::list<klib::weak_ptr<TaskDescriptor>>::iterator(), this);
    ptr->list_it = queue_list.push_back(task);

    

    if (task->queue_iterator) {
        task->queue_iterator->atomic_erase_from_parrent();
    }

    task->queue_iterator = klib::move(ptr);
}

void sched_pqueue::atomic_auto_push_front(const klib::shared_ptr<TaskDescriptor>& task)
{
    Auto_Lock_Scope scope_lock(lock);

    klib::unique_ptr<iterator> ptr = klib::make_unique<iterator>(klib::list<klib::weak_ptr<TaskDescriptor>>::iterator(), this);
    ptr->list_it = queue_list.push_front(task);

    

    if (task->queue_iterator) {
        task->queue_iterator->atomic_erase_from_parrent();
    }

    task->queue_iterator = klib::move(ptr);
}

void sched_pqueue::iterator::atomic_erase_from_parrent() noexcept
{
    sched_pqueue* parent = this->parent;
    Auto_Lock_Scope scope_lock(parent->lock);
    parent->queue_list.erase(list_it);
}

bool sched_pqueue::empty() const
{
    Auto_Lock_Scope scope_lock(lock);

    return queue_list.empty();
}
