#pragma once
#include <types.hh>

namespace kernel::x86::time
{

void init_timers();
void init_after_lapic();

/// Blocking timer waiter, for the stuff like MP init
class BlockingWaiter {
    BlockingWaiter() = default;
    unsigned tpr = 0;

public:
    ~BlockingWaiter();

    static BlockingWaiter create();
    void wait(u64 nanoseconds);
};

} // namespace kernel::x86::time