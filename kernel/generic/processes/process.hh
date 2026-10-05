#pragma once

#include "tasks.hh"
#include <types.hh>

namespace kernel::proc {

struct Process {
    using tasks_tree =
        pmos::containers::RedBlackTree<TaskDescriptor, &TaskDescriptor::process_tree_head,
                                        detail::TreeCmp<TaskDescriptor, u64, &TaskDescriptor::task_id>>::RBTreeHead;

    memory::RCU_Head rcu_head;

    u64 id = 0;

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