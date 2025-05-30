#include <kern_logger/kern_logger.hh>
#include <loongarch_asm.hh>
#include <sched/sched.hh>
#include <types.hh>

using namespace kernel;

constexpr u32 CPUCFG_LLFTP = (1 << 14);

static FreqFraction timer_freq;
static FreqFraction timer_period;

u64 ticks_since_bootup = 0;

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

    timer_freq   = computeFreqFraction((u64)constant_freq * mul, div);
    timer_period = computeFreqFraction(div, (u64)constant_freq * mul);
    return true;
}

void start_timer_oneshot(u64 ticks)
{
    ticks &= ~((u64)0x3);
    ticks |= 0x01; // Timer enable
    timer_tcfg(ticks);
}

void start_timer(u32 ms)
{
    auto c = sched::get_cpu_struct();

    const u64 ticks     = timer_freq * ms / 1000;
    const u64 timer_val = timer_value();
    start_timer_oneshot(ticks);
    if (c->timer_val > timer_val)
        c->timer_total += c->timer_val - timer_val;

    if (c->is_bootstap_cpu() && c->timer_val > timer_val) {
        ticks_since_bootup = c->timer_total;
    }
}

u64 get_current_time_ticks()
{
    auto c = sched::get_cpu_struct();
    return c->timer_total + c->timer_val - timer_value();
}

u64 sched::CPU_Info::ticks_after_ns(u64 ns) { return get_current_time_ticks() + (timer_freq * ns); }

u64 sched::get_ns_since_bootup() { return timer_period * ticks_since_bootup; }