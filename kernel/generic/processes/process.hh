#pragma once

#include "tasks.hh"
#include <types.hh>
#include <messaging/rights.hh>
#include <pmos/containers/intrusive_list.hh>

namespace kernel::proc {

struct Process;

struct ProcessRight final: ipc::Right {
    Process *process = nullptr;
    union {
        pmos::containers::DoubleListHead<ProcessRight> parent_head = {};
        memory::RCU_Head rcu_head;
    };

    static ReturnStr<ProcessRight *> create_for_group(Process *process, proc::TaskGroup *group);

    virtual ReturnStr<std::pair<ipc::Right *, u64>> duplicate(proc::TaskGroup *) override;
    virtual ipc::RightType type() const override;
    virtual void remove_from_parent() override;

    virtual void rcu_push() override;
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
    Spinlock lock;
    bool alive = true;
    tasks_tree child_tasks;

    static Process *create();

    void atomic_destroy_cleanup();

    kresult_t atomic_terminate();

    u64 get_id() const;
};



}