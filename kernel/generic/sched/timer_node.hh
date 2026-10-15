#pragma once
#include <pmos/containers/intrusive_bst.hh>
#include <types.hh>

namespace kernel::sched
{

struct TimerNode {
    pmos::containers::RBTreeNode<TimerNode> node;
    u64 fire_at_ns = 0;
    virtual void fire() = 0;
};

} // namespace kernel::sched