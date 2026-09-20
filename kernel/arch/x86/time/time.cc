#include <interrupts/apic.hh>
#include <sched/sched.hh>
#include <kern_logger/kern_logger.hh>
#include <x86_asm.hh>
#include "x86_timers.hh"
#include "acpi_pmtmr.hh"
#include "hpet.hh"
#include "tsc.hh"
#include "kvmclock.hh"
#include <time/timers.hh>

using namespace kernel::sched;
using namespace kernel;
using namespace kernel::x86::interrupts::lapic;
using namespace kernel::x86::time;
using namespace kernel::x86;
using namespace kernel::time;

u64 kernel::sched::ticks_since_bootup = 0;
void start_timer_ticks(u32 ticks)
{
    auto t = apic_get_remaining_ticks();
    apic_one_shot_ticks(ticks);
    auto c = get_cpu_struct();
    c->system_timer_val += c->timer_val - t;
    c->timer_val = ticks;

    if (c->is_bootstap_cpu())
        ticks_since_bootup = c->system_timer_val;
}
u64 get_current_time_ticks()
{
    auto c = get_cpu_struct();
    return c->system_timer_val + c->timer_val - apic_get_remaining_ticks();
}

extern bool have_invariant_tsc;
extern u64 boot_tsc;

u64 CPU_Info::ticks_after_ms(u64 ms) { return ticks_after_ns(ms * 1'000'000); }

struct TscTimer final: kernel::time::LocalTimer {
    virtual void set_deadline(u64 deadline_nanoseconds) override
    {
        arm_tsc_deadline(tsc::tsc_freq * deadline_nanoseconds);
    }

    virtual void cancel_deadline() override
    {
        arm_tsc_deadline(0);
    }

    virtual void init_as_main() override
    {
    }

    virtual const char *name() const override
    {
        return "TSC Deadline";
    }
};
TscTimer tsc_timer;

struct LapicTimer final: kernel::time::LocalTimer {
    virtual void set_deadline(u64 deadline_nanoseconds) override
    {
        auto current_time = get_ns_since_bootup();

        if (current_time > deadline_nanoseconds) {
            apic_one_shot_ticks(1);
        } else {
            auto diff = deadline_nanoseconds - current_time;
            auto time = apic_freq * diff + 1;
            if (time > UINT32_MAX) {
                time = UINT32_MAX;
            }

            apic_one_shot_ticks((u32)time);
        }
    }

    virtual void cancel_deadline() override
    {
        apic_one_shot_ticks(0);
    }

    virtual void init_as_main() override
    {
    }

    virtual const char *name() const override
    {
        return "APIC One-Shot";
    }
};
LapicTimer lapic_timer;

void kernel::x86::time::init_timers()
{
    // The timers are initialized, from worst to best, and set the kernel timeosource and cal
    // source as they do...
    acpi_pmtmr::init_acpi_pmtmr();
    hpet::init_hpet();
    tsc::init_tsc();
    //kvmclock::init_kvmclock();

    if (tsc::use_tsc_deadline())
        kernel_local_timer = &tsc_timer;
    else
        kernel_local_timer = &lapic_timer;

    if (!kernel_timesource)
        panic("No kernel timesource!\n");

    log::serial_logger.printf("Using %s as the kernel time source...\n", kernel_timesource->name());
    log::global_logger.printf("Using %s as the kernel time source...\n", kernel_timesource->name());
}

void x86::time::init_after_lapic()
{
    assert(kernel_timesource);
    kernel_timesource->init_as_main();
}

u64 CPU_Info::ticks_after_ns(u64 ns)
{
    return get_current_time_ticks() + (apic_freq * ns);
}

void TimeSource::init_as_main() {}
void CalibrationSource::prepare_for_calibration() {}
void CalibrationSource::end_calibration() {}

void BlockingWaiter::wait(u64 nanoseconds)
{
    u64 time = get_ns_since_bootup();
    u64 next = time + nanoseconds;

    do {
        if (tsc::use_tsc_deadline()) {
            arm_tsc_deadline(tsc::tsc_freq * next);
        } else {
            auto diff = next - time;
            auto time = apic_freq * diff + 1;
            if (time > UINT32_MAX) {
                time = UINT32_MAX;
            }

            apic_one_shot_ticks((u32)time);
        }

        asm("sti; hlt; cli;");

        time = get_ns_since_bootup();
    } while (time < next);
}

BlockingWaiter BlockingWaiter::create()
{
    BlockingWaiter w;

    lapic_timer_set_dummy();

    // Do this to protect against interrupts from IOAPICs
    w.tpr = tpr_read();
    tpr_write(14);

    return w;
}

BlockingWaiter::~BlockingWaiter()
{
    lapic_timer_restore_normal();

    tpr_write(tpr);
}