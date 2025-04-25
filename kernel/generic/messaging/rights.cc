#include "rights.hh"
#include "messaging.hh"
#include <sched/sched.hh>
#include <processes/task_group.hh>

namespace kernel::ipc
{

bool Right::destroy(proc::TaskGroup *match_group)
{
    Auto_Lock_Scope l(lock);

    if (!alive)
        return false;

    if (match_group && (of_message || parent_group != match_group))
        return false;

    alive = false;

    assert(parent);
    {
        Auto_Lock_Scope l(parent->rights_lock);
        parent->rights.erase(this);
    }

    if (!of_message) { // If it is of message, let it be garbage collected when the message is destroyed (in place of placeholders...)
        auto group = parent_group;
        assert(group);

        {
            Auto_Lock_Scope l(group->rights_lock);
            group->rights.erase(this);
        }

        rcu_head.rcu_func = [](void *self, bool) {
            Right *t =
                reinterpret_cast<Right *>(reinterpret_cast<char *>(self) - offsetof(Right, rcu_head));
            delete t;
        };
        sched::get_cpu_struct()->heap_rcu_cpu.push(&rcu_head);
    }

    return true;
}

ReturnStr<Right *> Right::create_for_group(Port *port, proc::TaskGroup *group, RightType type, u64 id_in_parent)
{
    assert(port);
    assert(group);
    assert(id_in_parent);

    klib::unique_ptr<Right> new_right = new Right();
    if (!new_right)
        return Error(-ENOMEM);

    new_right->parent = port;
    new_right->parent_group = group;
    new_right->of_message = false;
    new_right->right_parent_id = id_in_parent;

    // I don't know if this lock situation is good...
    Auto_Lock_Scope l(new_right->lock);

    Auto_Lock_Scope l1(port->rights_lock);
    Auto_Lock_Scope l2(group->rights_lock);

    if (!port->atomic_alive())
        return Error(-ENOENT);

    if (!group->atomic_alive())
        return Error(-ESRCH);

    new_right->right_sender_id = ++group->current_right_id;

    port->rights.insert(new_right.get());
    group->rights.insert(new_right.get());
    return Success(new_right.release());
}

}