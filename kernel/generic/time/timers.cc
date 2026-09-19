#include "timers.hh"
#include <sched/sched.hh>

using namespace kernel::time;

TimeSource *kernel::time::kernel_timesource = nullptr;
CalibrationSource *kernel::time::kernel_calibration_source = nullptr;
LocalTimer *kernel::time::kernel_local_timer = nullptr;

u64 kernel::sched::get_ns_since_bootup()
{
    if (!kernel_timesource)
        panic("No kernel timesource!!\n");

    return kernel_timesource->get_absolute_time();
}

void kernel::sched::maybe_rearm_timer(u64 deadline_nanoseconds)
{
    if (!kernel_local_timer)
        panic("No kernel local timer!!\n");

    auto c = get_cpu_struct();
    if (c->local_timer_next_deadline != 0 and (c->local_timer_next_deadline < deadline_nanoseconds))
        return;

    c->local_timer_next_deadline = deadline_nanoseconds;
    kernel_local_timer->set_deadline(deadline_nanoseconds);
}