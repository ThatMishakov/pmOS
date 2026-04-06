#include "rights.hh"

#include "messaging.hh"

#include <processes/task_group.hh>
#include <sched/sched.hh>
#include <memory/mem_object.hh>

namespace kernel::ipc
{

void SendRight::rcu_push()
{
    rcu_head.rcu_func = [](void *self, bool) {
        SendRight *t =
            reinterpret_cast<SendRight *>(reinterpret_cast<char *>(self) - offsetof(SendRight, rcu_head));
        delete t;
    };
    sched::get_cpu_struct()->heap_rcu_cpu.push(&rcu_head);
}

void MemObjectRight::rcu_push()
{
    rcu_head.rcu_func = [](void *self, bool) {
        MemObjectRight *t =
            reinterpret_cast<MemObjectRight *>(reinterpret_cast<char *>(self) - offsetof(MemObjectRight, rcu_head));
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

    remove_from_parent();

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

void SendRight::remove_from_parent()
{
    assert(parent);
    Auto_Lock_Scope l(parent->rights_lock);
    parent->rights.erase(this);
}

void MemObjectRight::remove_from_parent()
{
    assert(mem_object);
    Auto_Lock_Scope l(mem_object->rights_lock);
    mem_object->rights.remove(this);
}

bool Right::destroy_nolock()
{
    assert(lock.is_locked());

    if (!alive)
        return false;

    alive = false;

    remove_from_parent();

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

ReturnStr<SendRight *> SendRight::create_for_group(Port *port, proc::TaskGroup *group, RightType type,
                                           u64 id_in_parent)
{
    assert(port);
    assert(group);
    assert(id_in_parent);
    assert(type == RightType::SendOnce || type == RightType::SendMany);

    klib::unique_ptr<SendRight> new_right = new SendRight();
    if (!new_right)
        return Error(-ENOMEM);

    new_right->parent          = port;
    new_right->parent_group    = group;
    new_right->of_message      = false;
    new_right->right_parent_id = id_in_parent;
    new_right->send_many       = (type == RightType::SendMany);

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

ReturnStr<MemObjectRight *> MemObjectRight::create_for_group(klib::shared_ptr<paging::Mem_Object> mem_object,
                                            proc::TaskGroup *group, u32 permissions)
{
    // This is kinda copied from SendRight::create_for_group, and written very late into the night,
    // so it's probably worth having a second look at this in the future...
    assert(mem_object);
    assert(group);

    klib::unique_ptr<MemObjectRight> new_right = new MemObjectRight();
    if (!new_right)
        return Error(-ENOMEM);

    new_right->mem_object      = klib::move(mem_object);
    new_right->parent_group    = group;
    new_right->of_message      = false;
    new_right->permission_mask = permissions & PERM_ALL;
    

    Auto_Lock_Scope l(new_right->lock);

    Auto_Lock_Scope l1(mem_object->rights_lock);
    Auto_Lock_Scope l2(group->rights_lock);

    // The memory object should always be alive ... I think??

    if (!group->atomic_alive())
        return Error(-ESRCH);

    new_right->right_sender_id = ++group->current_right_id;

    mem_object->rights.push_back(new_right.get());
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

    right->right_0 = true;

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

ReturnStr<std::pair<Right *, u64>> SendRight::duplicate(proc::TaskGroup *group)
{
    if (!send_many)
        return Error(-EPERM);

    klib::unique_ptr<SendRight> new_right = new SendRight();
    if (!new_right)
        return Error(-ENOMEM);

    new_right->parent          = parent;
    new_right->parent_group    = parent_group;
    new_right->of_message      = false;
    new_right->right_parent_id = right_parent_id;
    new_right->send_many       = true;

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

    auto ptr = new_right.release();

    return Success(std::make_pair(ptr, ptr->right_sender_id));
}

ReturnStr<std::pair<Right *, u64>> MemObjectRight::duplicate(proc::TaskGroup *group)
{
    klib::unique_ptr<MemObjectRight> new_right = new MemObjectRight();
    if (!new_right)
        return Error(-ENOMEM);

    new_right->mem_object      = mem_object;
    new_right->parent_group    = parent_group;
    new_right->of_message      = false;
    new_right->permission_mask = permission_mask;

    Auto_Lock_Scope l(lock);
    if (!alive || of_message || parent_group != group)
        return Error(-ENOENT);

    Auto_Lock_Scope ll(new_right->lock);

    Auto_Lock_Scope l1(mem_object->rights_lock);
    Auto_Lock_Scope l2(parent_group->rights_lock);

    if (!parent_group->atomic_alive())
        return Error(-ESRCH);

    new_right->right_sender_id = ++parent_group->current_right_id;

    mem_object->rights.push_back(new_right.get());
    parent_group->rights.insert(new_right.get());

    auto ptr = new_right.release();

    return Success(std::make_pair(ptr, ptr->right_sender_id));
}

SendRight *to_send_right(Right *r)
{
    if (r->type() != RightType::SendOnce && r->type() != RightType::SendMany)
        return nullptr;

    return static_cast<SendRight *>(r);
}

RightType SendRight::type() const
{ 
    if (send_many)
        return RightType::SendMany;
    else
        return RightType::SendOnce;
}

RightType MemObjectRight::type() const
{
    return RightType::MemObject;
}

} // namespace kernel::ipc