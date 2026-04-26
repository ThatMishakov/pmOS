#include "interrupt_handler.hh"

#include <assert.h>
#include <errno.h>
#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>
#include <pmos/utility/scope_guard.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>

using namespace kernel::interrupts;

namespace kernel::interrupts
{

ReturnStr<IntSourceRight *> IntSourceRight::create_for_group(InterruptHandler *handler, proc::TaskGroup *group)
{
    assert(handler);
    assert(group);

    klib::unique_ptr<IntSourceRight> new_right = new IntSourceRight();
    if (!new_right)
        return Error(-ENOMEM);

    new_right->parent_handler = handler;
    new_right->parent_group = group;
    Auto_Lock_Scope l(new_right->lock);

    Auto_Lock_Scope l1(handler->sources_lock);
    Auto_Lock_Scope l2(group->rights_lock);
    
    if (!group->atomic_alive())
        return Error(-ESRCH);

    new_right->right_sender_id = ++group->current_right_id;
    handler->sources.push_back(new_right.get());
    group->rights.insert(new_right.get());
    return Success(new_right.release());
}

void IntSourceRight::rcu_push()
{
    rcu_head.rcu_func = [](void *self, bool) {
        IntSourceRight *t =
            reinterpret_cast<IntSourceRight *>(reinterpret_cast<char *>(self) - offsetof(IntSourceRight, rcu_head));
        delete t;
    };
    sched::get_cpu_struct()->heap_rcu_cpu.push(&rcu_head);
}

void IntSourceRight::remove_from_parent()
{
    assert(parent_handler);
    Auto_Lock_Scope l(parent_handler->sources_lock);
    parent_handler->sources.remove(this);
}

ipc::RightType IntSourceRight::type() const
{
    return ipc::RightType::InterruptSource;
}

ReturnStr<std::pair<ipc::Right *, u64>> IntSourceRight::duplicate(proc::TaskGroup *group)
{
    klib::unique_ptr<IntSourceRight> new_right = new IntSourceRight();
    if (!new_right)
        return Error(-ENOMEM);

    new_right->parent_group = group;
    new_right->parent_handler = parent_handler;

    Auto_Lock_Scope l(lock);
    if (!alive || of_message || parent_group != group)
        return Error(-ENOENT);

    Auto_Lock_Scope ll(new_right->lock);

    Auto_Lock_Scope l1(parent_handler->sources_lock);
    Auto_Lock_Scope l2(group->rights_lock);

    if (!group->atomic_alive())
        return Error(-ESRCH);

    new_right->right_sender_id = ++group->current_right_id;
    parent_handler->sources.push_back(new_right.get());
    group->rights.insert(new_right.get());
    auto ptr = new_right.release();
    return Success(std::make_pair(ptr, ptr->right_sender_id));
}

NotificationResult InterruptHandler::send_interrupt_notification()
{
    if (notification_rights.empty())
        return NotificationResult::NoHandlers;

    for (auto &n: notification_rights) {
        if (n.sent)
            continue;

        auto port = n.parent;
        assert(port);
        // Only the owner can delete the port, so this isn't possible
        assert(port->alive);

        n.pending_completion = true;

        Auto_Lock_Scope l(port->lock);
        port->enqueue(klib::unique_ptr(&n));
    }
    
    return NotificationResult::Success;
}

kresult_t IntNotificationRight::complete()
{
    assert(alive);
    if (!pending_completion)
        return -EAGAIN;

    pending_completion = false;
 
    assert(parent_handler);
    assert(sched::get_cpu_struct() == parent_handler->parent_cpu);

    bool all_completed = true;
    for (auto &n: parent_handler->notification_rights) {
        if (n.pending_completion) {
            all_completed = false;
            break;
        }
    }

    if (all_completed)
        interrupt_complete(parent_handler);

    return 0;
}

}