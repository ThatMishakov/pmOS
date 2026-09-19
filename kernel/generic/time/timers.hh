#pragma once
#include <pmos/containers/vector.hh>
#include <types.hh>

namespace kernel::time
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

struct LocalTimer {
    virtual void set_deadline(u64 deadline_nanoseconds) = 0;
    virtual void cancel_deadline()                      = 0;
    virtual void init_as_main()                         = 0;
    virtual const char *name() const                    = 0;
};

extern TimeSource *kernel_timesource;
extern CalibrationSource *kernel_calibration_source;
extern LocalTimer *kernel_local_timer;

} // namespace kernel::time