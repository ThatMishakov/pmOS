#include "process.hh"

namespace kernel::proc {

static AtomicCounter next_id;

Process *Process::create()
{
    auto p = new Process();
    if (!p)
        return nullptr;

    p->id = next_id.atomic_next();

    return p;
}

void Process::atomic_destroy_cleanup()
{
    assert(!alive);

    auto front_right = [&]() -> ProcessRight * {
        Auto_Lock_Scope l(lock);
        if (rights.empty())
            return nullptr;
        return &rights.front();
    };

    while (auto right = front_right()) {
        right->destroy(ProcessRight::DestroyReason::DeletedByReceiver);
    }

    rcu_head.rcu_func = [](void *self, bool) {
        Process *p = reinterpret_cast<Process *>(reinterpret_cast<char *>(self) - offsetof(Process, rcu_head));
        delete p;
    };
    sched::get_cpu_struct()->heap_rcu_cpu.push(&rcu_head);
}

kresult_t Process::atomic_terminate()
{
    Auto_Lock_Scope l(lock);
    if (!alive)
        return -ESRCH;

    alive = false;
    for (auto &t: child_tasks)
        t.atomic_kill(false);
    return 0;
}

u64 Process::get_id() const
{
    return id;
}

ReturnStr<ProcessRight *> ProcessRight::create_for_group(Process *process, proc::TaskGroup *group)
{
    assert(process);
    assert(group);

    auto new_right = klib::unique_ptr<ProcessRight>(new ProcessRight());
    if (!new_right)
        return Error(-ENOMEM);

    new_right->process      = process;
    new_right->parent_group = group;
    new_right->of_message   = false;

    Auto_Lock_Scope l(new_right->lock);

    Auto_Lock_Scope l2(group->rights_lock);
    if (!group->atomic_alive())
        return Error(-ESRCH);

    Auto_Lock_Scope l3(process->lock);
    if (!process->alive)
        return Error(-ESRCH);

    new_right->right_sender_id = ++group->current_right_id;
    group->rights.insert(new_right.get());
    process->rights.push_back(new_right.get());

    auto ptr = new_right.release();

    return Success(ptr);
}

ipc::RightType ProcessRight::type() const
{
    return ipc::RightType::Process;
}

void ProcessRight::remove_from_parent()
{
    assert(process);
    Auto_Lock_Scope l(process->lock);
    process->rights.erase(this);
}

void ProcessRight::rcu_push()
{
    rcu_head.rcu_func = [](void *self, bool) {
        ProcessRight *t =
            reinterpret_cast<ProcessRight *>(reinterpret_cast<char *>(self) - offsetof(ProcessRight, rcu_head));
        delete t;
    };
    sched::get_cpu_struct()->heap_rcu_cpu.push(&rcu_head);
}

ReturnStr<std::pair<ipc::Right *, u64>> ProcessRight::duplicate(proc::TaskGroup *group)
{
    assert(process);
    assert(parent_group);
    assert(parent_group == group);

    klib::unique_ptr<ProcessRight> new_right = new ProcessRight();
    if (!new_right)
        return Error(-ENOMEM);

    new_right->process      = process;
    new_right->parent_group = parent_group;
    new_right->of_message   = false;
    new_right->permissions_mask = permissions_mask;

    Auto_Lock_Scope l(lock);
    if (!alive || of_message || parent_group != group)
        return Error(-ENOENT);

    Auto_Lock_Scope ll(new_right->lock);

    Auto_Lock_Scope l1(process->lock);
    if (!process->alive)
        return Error(-ENOENT);

    Auto_Lock_Scope l2(parent_group->rights_lock);
    if (!parent_group->atomic_alive())
        return Error(-ESRCH);

    new_right->right_sender_id = ++parent_group->current_right_id;
    parent_group->rights.insert(new_right.get());
    process->rights.push_back(new_right.get());

    auto ptr = new_right.release();

    return Success(std::make_pair(ptr, ptr->right_sender_id));
}

bool Process::atomic_alive() const
{
    Auto_Lock_Scope l(lock);
    return alive;
}

} // namespace kernel::proc