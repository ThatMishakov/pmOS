#include "rights.hh"

#include "messaging.hh"

#include <processes/task_group.hh>
#include <sched/sched.hh>
#include <memory/mem_object.hh>
#include <pmos/ipc.h>


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

bool Right::destroy(Right::DestroyReason reason, proc::TaskGroup *match_group)
{
    Auto_Lock_Scope l(lock);

    return destroy_nolock(reason, match_group);
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

bool Right::destroy_nolock(Right::DestroyReason, proc::TaskGroup *match_group)
{
    assert(lock.is_locked());

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

bool SendOnceRight::destroy_nolock(DestroyReason reason, proc::TaskGroup *match_group)
{
    assert(lock.is_locked());
    
    if (!alive)
        return false;

    if (match_group && (of_message || parent_group != match_group))
        return false;

    alive = false;

    remove_from_parent();

    bool send_notification = reason == DestroyReason::DeletedBySender;

    if (!of_message) {
        auto group = parent_group;
        assert(group);

        {
            Auto_Lock_Scope l(group->rights_lock);
            group->rights.erase(this);
        }
    }

    if (!send_notification and !of_message) {
        rcu_push();
    }

    if (send_notification) {
        auto port = parent;
        assert(port);

        {
            Auto_Lock_Scope l(port->lock);
            port->enqueue(klib::unique_ptr<SendRight>(this));
        }

        sent = true;
    }

    return true;
}

void SendOnceRight::delete_self()
{
    Auto_Lock_Scope l(lock);
    
    assert(!alive);
    assert(sent);
    sent = false; // Clear this so that if it's held by a message, it can be freed

    if (!of_message) {
        rcu_push();
    }
}

void SendManyRight::delete_self()
{
    // At this point, the right was already deleted, and sent, so don't do anything special

    Auto_Lock_Scope l(lock);
    
    assert(!alive);
    assert(sent);
    sent = false;

    if (!of_message) {
        rcu_push();
    }
}

ReturnStr<SendOnceRight *> SendOnceRight::create_for_group(Port *port, proc::TaskGroup *group, u64 id_in_parent)
{
    assert(port);
    assert(group);
    assert(id_in_parent);

    auto new_right = klib::unique_ptr<SendOnceRight>(new SendOnceRight());
    if (!new_right)
        return Error(-ENOMEM);

    new_right->parent          = port;
    new_right->parent_group    = group;
    new_right->of_message      = false;
    new_right->right_parent_id = id_in_parent;

    Auto_Lock_Scope l(new_right->lock);

    // I don't know if this lock situation is good...
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

ReturnStr<SendManyRight *> SendManyRight::create_for_group(Port *port, proc::TaskGroup *group, u64 id_in_parent)
{
    assert(port);
    assert(group);
    assert(id_in_parent);

    auto new_right = klib::unique_ptr<SendManyRight>(new SendManyRight());
    if (!new_right)
        return Error(-ENOMEM);

    auto shared = klib::make_unique<SendManyRightShared>();
    if (!shared)
        return Error(-ENOMEM);

    new_right->shared = shared.get();
    shared->send_many_rights.push_back(new_right.get());

    shared->parent          = port;
    shared->right_parent_id = id_in_parent;

    new_right->parent_group = group;
    new_right->of_message   = false;

    Auto_Lock_Scope l(new_right->lock);

    Auto_Lock_Scope ll(shared->lock);

    Auto_Lock_Scope l1(port->rights_lock);
    Auto_Lock_Scope l2(group->rights_lock);

    if (!port->atomic_alive())
        return Error(-ENOENT);

    if (!group->atomic_alive())
        return Error(-ESRCH);

    new_right->right_sender_id = ++group->current_right_id;

    port->rights.insert(shared.release());
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

    new_right->mem_object      = mem_object;
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
    destroy(DestroyReason::DeletedByReceiver);
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
    assert(from);
    assert(to);

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

ReturnStr<std::pair<Right *, u64>> SendOnceRight::duplicate(proc::TaskGroup *)
{
    return Error(-EPERM);
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
    assert(r);
    if (r->type() != RightType::SendOnce && r->type() != RightType::SendMany)
        return nullptr;

    return static_cast<SendRight *>(r);
}

RightType SendOnceRight::type() const
{ 
    return RightType::SendOnce;
}

RightType SendManyRight::type() const
{
    return RightType::SendMany;
}

RightType MemObjectRight::type() const
{
    return RightType::MemObject;
}

unsigned Right::type_as_int() const
{
    switch (type()) {
    case RightType::SendOnce:
        return 1;
    case RightType::SendMany:
        return 2;
    case RightType::MemObject:
        return 3;
    default:
        return 0;
    }
}

u64 RecieveRight::sender_task_id() const
{
    return 0; // Kernel
}

u64 RecieveRight::sent_with_right() const
{
    return right_parent_id;
}

static IPC_Kernel_Recieve_Right_Destroyed destroyed_msg = {
    .type  = IPC_Kernel_Recieve_Right_Destroyed_NUM,
    .flags = 0,
};

size_t RecieveRight::size() const
{
    return sizeof(destroyed_msg);
}

ReturnStr<bool> RecieveRight::copy_to_user_buff(char *buff) const
{
    return copy_to_user(reinterpret_cast<const char *>(&destroyed_msg), buff, sizeof(destroyed_msg));
}

bool SendRight::destroy_recieve_right()
{
    return destroy(DestroyReason::DeletedByReceiver);
}

klib::unique_ptr<SendRight> SendOnceRight::create_for_message()
{
    return new SendOnceRight();
}

Port *SendOnceRight::parent_port()
{
    assert(parent);
    return parent;
}

Port *SendManyRight::parent_port()
{
    assert(shared);
    assert(shared->parent);
    return shared->parent;
}

std::pair<klib::unique_ptr<SendRight>, klib::unique_ptr<RecieveRight>> SendManyRight::create_for_message()
{
    auto send_right = klib::make_unique<SendManyRight>();
    if (!send_right)
        return {};

    auto shared = klib::make_unique<SendManyRightShared>();
    if (!shared)
        return {};

    send_right->shared = shared.get();

    // Port and ID get set by the message
    // shared->parent = port;
    // shared->right_parent_id = id_in_parent;

    // Same deal as with the message...

    // Also, since these objects can't be accessed by anyone here, so locks are not needed...
    shared->send_many_rights.push_back(send_right.get());
    return {std::move(send_right), std::move(shared)};
}

ReturnStr<std::pair<Right *, u64>> SendManyRight::duplicate(proc::TaskGroup *group)
{
    klib::unique_ptr<SendManyRight> new_right = new SendManyRight();
    if (!new_right)
        return Error(-ENOMEM);

    assert(shared);
    assert(parent_group);

    new_right->shared       = shared;
    new_right->parent_group = parent_group;
    new_right->of_message   = false;

    auto parent = shared->parent;
    assert(parent);

    Auto_Lock_Scope ll(new_right->lock);

    Auto_Lock_Scope l(lock);
    if (!alive || of_message || parent_group != group)
        return Error(-ENOENT);

    Auto_Lock_Scope ls(shared->lock);
    if (!shared->alive)
        return Error(-ENOENT);

    Auto_Lock_Scope l2(parent_group->rights_lock);

    if (!parent->atomic_alive())
        return Error(-ENOENT);

    if (!parent_group->atomic_alive())
        return Error(-ESRCH);

    shared->send_many_rights.push_back(new_right.get());

    new_right->right_sender_id = ++parent_group->current_right_id;

    parent->rights.insert(new_right.get());
    parent_group->rights.insert(new_right.get());

    auto ptr = new_right.release();

    return Success(std::make_pair(ptr, ptr->right_sender_id));
}

void SendManyRightShared::rcu_push()
{
    rcu_head.rcu_func = [](void *self, bool) {
        SendManyRightShared *t =
            reinterpret_cast<SendManyRightShared *>(reinterpret_cast<char *>(self) - offsetof(SendManyRightShared, rcu_head));
        delete t;
    };
    sched::get_cpu_struct()->heap_rcu_cpu.push(&rcu_head);
}

bool SendManyRightShared::destroy_recieve_right()
{
    // Note: the locking situation here is difficult. But basically, the rules are that you don't attempt
    // to acquire a lock on a right while holding the lock on this struct
    {
        Auto_Lock_Scope l(lock);
        if (!alive)
            return false;

        alive = false;
    }

    auto get_front = [&]() -> SendManyRight * {
        Auto_Lock_Scope l(lock);
        if (send_many_rights.empty())
            return nullptr;
        return &send_many_rights.front();
    };

    while (auto right = get_front()) {
        right->destroy(Right::DestroyReason::DeletedBySender);
    }

    // Here would be the best place to enqueue this right into a port. But I've decided not to, since
    // when this function gets called, it is either when the channel is dying, at which point this
    // message won't be read, or because youserspace is calling this explicitly, at which point
    // you don't have to have notifications.
    //
    // It might have been useful to use this to know when you've recieved the last message on the right,
    // but at this point, this wasn't needed, abd if someone is deleting this right, it id because
    // the caller doesn't want to recieve any more recieve messages on the right...

    rcu_push();
    return true;
}

bool SendManyRight::destroy_nolock(DestroyReason reason, proc::TaskGroup *match_group)
{
    assert(lock.is_locked());

    if (!alive)
        return false;

    if (match_group && (of_message || parent_group != match_group))
        return false;

    alive = false;

    bool send_shared = false;
    assert(shared);

    {
        Auto_Lock_Scope l(shared->lock);
        assert(!shared->send_many_rights.empty());
        shared->send_many_rights.remove(this);

        if (alive && shared->send_many_rights.empty()) {
            alive = false;
            send_shared = true;
        }
    }

    // TODO
    bool send_notification = false;

    if (!of_message) {
        auto group = parent_group;
        assert(group);

        {
            Auto_Lock_Scope l(group->rights_lock);
            group->rights.erase(this);
        }
    }

    if (!send_notification and !of_message) {
        rcu_push();
    }

    if (send_notification) {
        auto port = parent;
        assert(port);

        {
            Auto_Lock_Scope l(port->lock);
            port->enqueue(klib::unique_ptr<GenericMessage>(this));
        }

        sent = true;
    }

    if (send_shared) {
        auto port = shared->parent;
        assert(port);

        {
            Auto_Lock_Scope l(port->lock);
            port->enqueue(klib::unique_ptr<GenericMessage>(shared));
        }
    }

    return true;
}

void SendManyRightShared::delete_self()
{   
    {
        Auto_Lock_Scope l(lock);
        assert(!alive);
        assert(send_many_rights.empty());
    }

    rcu_push();
}

} // namespace kernel::ipc