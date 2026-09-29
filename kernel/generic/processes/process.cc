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

} // namespace kernel::proc