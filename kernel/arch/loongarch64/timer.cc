#include <kern_logger/kern_logger.hh>
#include <loongarch_asm.hh>
#include <sched/sched.hh>
#include <types.hh>

using namespace kernel;

constexpr u32 CPUCFG_LLFTP = (1 << 14);

static FreqFraction timer_freq;
static FreqFraction timer_period;

u64 ticks_since_bootup = 0;

u32 tmrbits = 0;
u64 timer_max;

bool calculate_timer_frequency()
{
    auto cpucfg2 = cpucfg(2);
    if (!(cpucfg2 & CPUCFG_LLFTP)) {
        log::serial_logger.printf("CPU has no constant frequency timer...\n");
        return false;
    }

    u32 constant_freq = cpucfg(4);
    u32 mul_div       = cpucfg(5);
    u32 mul           = mul_div & 0xffff;
    u32 div           = mul_div >> 16;

    tmrbits = (csrrd32<0x21>() >> 4) & 0xff;
    timer_max = (1 << tmrbits) - 1;

    log::serial_logger.printf("CPU Timer freq %i mul %i div %i bits %i\n", constant_freq, mul_div, mul, div, tmrbits);

    timer_freq   = computeFreqFraction((u64)constant_freq * mul, div * 1e9);
    timer_period = computeFreqFraction(div * 1e9, (u64)constant_freq * mul);
    return true;
}

void start_timer_oneshot(u64 ticks)
{
    ticks &= ~((u64)0x3);
    ticks |= 0x01; // Timer enable
    timer_tcfg(ticks);
}

// void start_timer(u32 ms)
// {
//     auto c = sched::get_cpu_struct();

//     const u64 ticks     = timer_freq * ms / 1000;
//     const u64 timer_val = timer_value();
//     start_timer_oneshot(ticks);
//     if (c->timer_val > timer_val)
//         c->timer_total += c->timer_val - timer_val;

//     if (c->is_bootstap_cpu() && c->timer_val > timer_val) {
//         ticks_since_bootup = c->timer_total;
//     }
// }

u64 get_current_time_ticks()
{
    return rdtimed().stable_counter;
}

void kernel::sched::maybe_rearm_timer(u64 deadline_nanoseconds)
{
    auto c = get_cpu_struct();
    if (c->local_timer_next_deadline != 0 and (c->local_timer_next_deadline < deadline_nanoseconds))
        return;

    c->local_timer_next_deadline = deadline_nanoseconds;
    u64 wanted_ticks = timer_freq * deadline_nanoseconds;
    u64 time = get_current_time_ticks();

    if (wanted_ticks < time) {
        start_timer_oneshot(100);
    } else {
        u64 diff = wanted_ticks - time;
        if (diff > timer_max) {
            start_timer_oneshot(timer_max);
        } else {
            start_timer_oneshot(diff);
        }
    }
}

u64 sched::get_ns_since_bootup() { return timer_period * rdtimed().stable_counter; }