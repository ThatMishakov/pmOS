#pragma once
#include <lib/vector.hh>
#include <types.hh>

namespace kernel::x86::time
{

struct TimeSource {
    virtual u64 get_absolute_time() const = 0;
    virtual void init_as_main();
    virtual const char *name() const = 0;
};

// The prepare and end functions there are so that the timers can (HPET) can start and stop their counters, without
// factoring that time into the calibration time itself...
struct CalibrationSource {
    virtual void prepare_for_calibration();
    virtual u64 wait_for_nanoseconds(u64 time_nanoseconds) const = 0;
    virtual void end_calibration();
    virtual const char *name() const                             = 0;
};

extern TimeSource *kernel_timesource;
extern CalibrationSource *kernel_calibration_source;

void init_timers();
void init_after_lapic();

/// Blocking timer waiter, for the stuff like MP init
class BlockingWaiter {
    BlockingWaiter() = default;
    unsigned iopl = 0;

public:
    ~BlockingWaiter();

    static BlockingWaiter create();
    void wait(u64 nanoseconds);
};

} // namespace kernel::x86::time