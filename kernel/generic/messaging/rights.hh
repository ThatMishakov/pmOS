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
    Spinlock lock;

    bool destroy(proc::TaskGroup *match_group = nullptr);
    bool destroy_nolock();
    void destroy_deleting_message();

    void rcu_push();

    static ReturnStr<Right *> create_for_group(Port *port, proc::TaskGroup *group, RightType type,
                                               u64 id_in_parent);

    bool of_group(proc::TaskGroup *) const;
};

} // namespace kernel::ipc