#pragma once

#include <memory/rcu.hh>
#include <pmos/containers/intrusive_bst.hh>
#include <types.hh>

namespace kernel::proc
{
class TaskGroup;
}

namespace kernel::ipc
{

struct Message;
class Port;

enum class RightType : bool {
    SendOnce,
    SendMany,
};

struct Right {
    // Parent port
    Port *parent = nullptr;

    union {
        Message *parent_message = nullptr;
        proc::TaskGroup *parent_group;
    };

    union {
        pmos::containers::RBTreeNode<Right> parent_port_head = {};
        memory::RCU_Head rcu_head;
    };

    pmos::containers::RBTreeNode<Right> task_group_head;

    /// Sender-facing id (gets changed when the right is copied/moved, depending on the task group)
    u64 right_sender_id = 0;
    /// Parent-facing id (does not change, and gets copied when right is duplicated)
    u64 right_parent_id = 0;

    RightType type : 1  = RightType::SendOnce;
    bool alive : 1      = true;
    bool of_message : 1 = false;
    bool right_0 : 1    = false;
    mutable Spinlock lock;

    bool destroy(proc::TaskGroup *match_group = nullptr);
    bool destroy_nolock();
    void destroy_deleting_message();

    void rcu_push();

    static ReturnStr<Right *> create_for_group(Port *port, proc::TaskGroup *group, RightType type,
                                               u64 id_in_parent);

    bool of_group(proc::TaskGroup *) const;
    bool atomic_alive() const;

    ReturnStr<u64> atomic_transfer_to_group(proc::TaskGroup *from, proc::TaskGroup *to);
    // Atomically creates and returns right and its id in sender
    ReturnStr<std::pair<Right *, u64>> duplicate(proc::TaskGroup *);
};

u64 atomic_right0_id();
kresult_t set_right0(Right *right, proc::TaskGroup *right_parent);

} // namespace kernel::ipc