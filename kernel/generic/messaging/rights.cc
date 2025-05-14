#include "rights.hh"

#include "messaging.hh"

#include <processes/task_group.hh>
#include <sched/sched.hh>

namespace kernel::ipc
{

void Right::rcu_push()
{
    rcu_head.rcu_func = [](void *self, bool) {
        Right *t =
            reinterpret_cast<Right *>(reinterpret_cast<char *>(self) - offsetof(Right, rcu_head));
        delete t;
    };
    sched::get_cpu_struct()->heap_rcu_cpu.push(&rcu_head);
}

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

    if (!of_message) { // If it is of message, let it be garbage collected when the message is
                       // destroyed (in place of placeholders...)
        auto group = parent_group;
        assert(group);

        {
            Auto_Lock_Scope l(group->rights_lock);
            group->rights.erase(this);
        }

        rcu_push();
    }

    return true;
}

bool Right::destroy_nolock()
{
    assert(lock.is_locked());

    if (!alive)
        return false;

    alive = false;

    assert(parent);
    {
        Auto_Lock_Scope l(parent->rights_lock);
        parent->rights.erase(this);
    }

    if (!of_message) { // If it is of message, let it be garbage collected when the message is
                       // destroyed (in place of placeholders...)
        auto group = parent_group;
        assert(group);

        {
            Auto_Lock_Scope l(group->rights_lock);
            group->rights.erase(this);
        }

        rcu_push();
    }

    return true;
}

ReturnStr<Right *> Right::create_for_group(Port *port, proc::TaskGroup *group, RightType type,
                                           u64 id_in_parent)
{
    assert(port);
    assert(group);
    assert(id_in_parent);

    klib::unique_ptr<Right> new_right = new Right();
    if (!new_right)
        return Error(-ENOMEM);

    new_right->parent          = port;
    new_right->parent_group    = group;
    new_right->of_message      = false;
    new_right->right_parent_id = id_in_parent;
    new_right->type            = type;

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

bool Right::of_group(proc::TaskGroup *g) const { return !of_message && parent_group == g; }

void Right::destroy_deleting_message()
{
    destroy();
    assert(of_message);
    rcu_push();
}

bool Right::atomic_alive() const
{
    Auto_Lock_Scope l(lock);
    return alive;
}

static Spinlock right0_lock;
static u64 right0_id = 0;

u64 atomic_right0_id()
{
    Auto_Lock_Scope l(right0_lock);
    return right0_id;
}

kresult_t set_right0(Right *right, proc::TaskGroup *right_parent)
{
    Auto_Lock_Scope l(right0_lock);

    if (auto current = proc::kernel_tasks->atomic_get_right(right0_id);
        current && current->atomic_alive())
        return -EEXIST;

    auto result = right->atomic_transfer_to_group(right_parent, proc::kernel_tasks);
    if (!result.success())
        return result.result;

    right0_id = result.val;
    return 0;
}

ReturnStr<u64> Right::atomic_transfer_to_group(proc::TaskGroup *from, proc::TaskGroup *to)
{
    Auto_Lock_Scope l(lock);
    if (!alive)
        return -ENOENT;

    if (of_message || parent_group != from)
        return -ENOENT;

    Auto_Lock_Scope_Double ll(from->rights_lock, to->rights_lock);
    if (from == to)
        return Success(right_sender_id);

    if (!to->atomic_alive())
        return -ESRCH;

    auto new_id = ++to->current_right_id;

    from->rights.erase(this);

    parent_group    = to;
    right_sender_id = new_id;

    to->rights.insert(this);

    return Success(new_id);
}

ReturnStr<std::pair<Right *, u64>> Right::duplicate(proc::TaskGroup *group)
{
    if (type != RightType::SendMany)
        return Error(-EPERM);

    klib::unique_ptr<Right> new_right = new Right();
    if (!new_right)
        return Error(-ENOMEM);

    new_right->parent          = parent;
    new_right->parent_group    = parent_group;
    new_right->of_message      = false;
    new_right->right_parent_id = right_parent_id;
    new_right->type            = type;

    Auto_Lock_Scope l(lock);
    if (!alive || of_message || parent_group != group)
        return Error(-ENOENT);

    Auto_Lock_Scope ll(new_right->lock);

    Auto_Lock_Scope l1(parent->rights_lock);
    Auto_Lock_Scope l2(parent_group->rights_lock);

    if (!parent->atomic_alive())
        return Error(-ENOENT);

    if (!parent_group->atomic_alive())
        return Error(-ESRCH);

    new_right->right_sender_id = ++parent_group->current_right_id;

    parent->rights.insert(new_right.get());
    parent_group->rights.insert(new_right.get());

    return Success(std::make_pair(new_right.release(), new_right->right_sender_id));
}

} // namespace kernel::ipc