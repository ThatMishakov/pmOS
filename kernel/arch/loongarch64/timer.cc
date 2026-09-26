#include <kern_logger/kern_logger.hh>
#include <loongarch_asm.hh>
#include <sched/sched.hh>
#include <types.hh>
#include <time/timers.hh>

using namespace kernel;

constexpr u32 CPUCFG_LLFTP = (1 << 14);

static FreqFraction timer_freq;
static FreqFraction timer_period;

u64 ticks_since_bootup = 0;

u32 tmrbits = 0;
u64 timer_max;

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

struct LoongArchTimer final: kernel::time::LocalTimer {
    virtual void set_deadline(u64 deadline_nanoseconds) override
    {
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

    virtual void cancel_deadline() override
    {
        start_timer_oneshot(0);
    }

    virtual void init_as_main() override
    {
    }

    virtual const char *name() const override
    {
        return "LoongArch Timer";
    }
};
LoongArchTimer loongarch_timer;

struct LoongArchTimeSource final: kernel::time::TimeSource {
    virtual u64 get_absolute_time() const override
    {
        return timer_period * get_current_time_ticks();
    }

    virtual const char *name() const override
    {
        return "LoongArch Timer";
    }

    virtual void init_as_main() override
    {
    }
};
LoongArchTimeSource loongarch_time_source;

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

    log::serial_logger.printf("CPU Timer freq %i mul %i div %i bits %i\n", constant_freq, mul, div, tmrbits);

    timer_freq   = computeFreqFraction((u64)constant_freq * mul, div * (u64)1'000'000'000);
    timer_period = computeFreqFraction(div * (u64)1'000'000'000, (u64)constant_freq * mul);

    kernel::time::kernel_timesource = &loongarch_time_source;
    kernel::time::kernel_local_timer = &loongarch_timer;

    return true;
}