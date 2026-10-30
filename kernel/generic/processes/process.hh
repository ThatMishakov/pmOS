#pragma once

#include "tasks.hh"
#include <types.hh>
#include <messaging/rights.hh>
#include <pmos/containers/intrusive_list.hh>
#include <optional>

namespace kernel::proc {

struct Process;

struct ProcessRight final: ipc::Right, ipc::ReceiveRight {
    Process *process = nullptr;
    // Notification port
    ipc::Port *port = nullptr;
    union {
        pmos::containers::DoubleListHead<ProcessRight> parent_head = {};
        memory::RCU_Head rcu_head;
    };
    u64 saved_process_id = 0;
    std::optional<i32> exit_code;

    static ReturnStr<ProcessRight *> create_for_group(Process *process, proc::TaskGroup *group);

    virtual ReturnStr<std::pair<ipc::Right *, u64>> duplicate(proc::TaskGroup *) override;
    virtual ipc::RightType type() const override;
    virtual void remove_from_parent() override;

    virtual void rcu_push() override;

    virtual bool destroy_nolock(ipc::Right::DestroyReason reason, proc::TaskGroup *match_group = nullptr) override;

    // ReceiveRight overrides
    virtual bool destroy_receive_right() override;
    virtual ipc::RightType receive_type() const override;

    // GenericMessage overrides
    virtual void delete_self() override;
    virtual size_t size() const override;
    virtual ReturnStr<bool> copy_to_user_buff(char *buff, size_t max_size) const override;
};

struct Process {
    using tasks_tree =
        pmos::containers::RedBlackTree<TaskDescriptor, &TaskDescriptor::process_tree_head,
                                        detail::TreeCmp<TaskDescriptor, u64, &TaskDescriptor::task_id>>::RBTreeHead;

    memory::RCU_Head rcu_head;

    u64 id = 0;

    using rights_list = pmos::containers::CircularDoubleList<ProcessRight, &ProcessRight::parent_head>;
    rights_list rights;

    // Locking order: Don't take this lock while holding of any of the
    // child tasks
    mutable Spinlock lock;
    bool alive = true;
    tasks_tree child_tasks;
    i32 exit_code{};

    static Process *create();

    void atomic_destroy_cleanup();

    kresult_t atomic_terminate(i32 exit_code);

    u64 get_id() const;

    bool atomic_alive() const;
};



}