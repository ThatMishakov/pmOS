#include "interrupt_handler.hh"

#include <assert.h>
#include <errno.h>
#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>
#include <pmos/utility/scope_guard.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <pmos/ipc.h>

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

ipc::RightType IntNotificationRight::recieve_type() const
{
    return ipc::RightType::InterruptNotification;
}

size_t IntNotificationRight::size() const
{
    return sizeof(IPC_Kernel_Interrupt);
}

ReturnStr<bool> IntNotificationRight::copy_to_user_buff(char *buff) const
{
    IPC_Kernel_Interrupt interrupt = {
        .type = IPC_Kernel_Interrupt_NUM,
    };

    return copy_to_user((const char *)&interrupt, buff, sizeof(interrupt));
}

// Didn't like having a private struct member
static void after_removing_pending(InterruptHandler *handler)
{
    assert(handler);
    assert(sched::get_cpu_struct() == handler->parent_cpu);

    bool all_completed = true;
    for (auto &n: handler->notification_rights) {
        if (n.pending_completion) {
            all_completed = false;
            break;
        }
    }

    if (all_completed)
        interrupt_complete(handler);

    if (handler->notification_rights.empty())
        interrupt_disable(handler);
}

bool IntNotificationRight::destroy_recieve_right()
{
    if (!alive)
        return false;

    alive = false;

    assert(parent);
    parent->atomic_remove_right(this);

    auto parent_task = parent->owner;
    assert(parent_task);

    {
        Auto_Lock_Scope l(parent_task->sched_lock);
        assert(parent_task->interrupt_handlers_count > 0);
        parent_task->interrupt_handlers_count--;
    }

    assert(parent_handler);
    parent_handler->notification_rights.remove(this);

    if (pending_completion)
        after_removing_pending(parent_handler);

    if (!sent)
        delete this;

    return true;
}

void IntNotificationRight::delete_self()
{
    sent = false;
    if (!alive)
        delete this;
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

ReturnStr<IntNotificationRight *> IntNotificationRight::create_for_port(InterruptHandler *handler, ipc::Port *port)
{
    bool first_right;

    assert(handler);
    assert(port);

    auto parent_task = port->owner;
    assert(parent_task);

    auto cpu = handler->parent_cpu;
    assert(cpu);

    assert(sched::get_current_task() == parent_task);

    klib::unique_ptr<IntNotificationRight> new_right = new IntNotificationRight();
    if (!new_right)
        return Error(-ENOMEM);

    new_right->parent_handler = handler;
    new_right->parent = port;

    {
        Auto_Lock_Scope l3(parent_task->sched_lock);
        if (parent_task->cpu_affinity != (cpu->cpu_id + 1))
            return Error(-EINTR);

        assert(sched::get_cpu_struct()->cpu_id == cpu->cpu_id);
        assert(sched::get_cpu_struct() == cpu);

        Auto_Lock_Scope l1(handler->sources_lock);
        Auto_Lock_Scope l2(port->rights_lock);

        first_right = handler->notification_rights.empty();

        assert(port->alive);
        new_right->right_parent_id = port->new_right_id();
        handler->notification_rights.push_back(new_right.get());
        port->rights.insert(new_right.get());

        parent_task->interrupt_handlers_count++;
    }

    if (first_right)
        interrupt_enable(handler);

    return Success(new_right.release());
}

}