#include "tsc.hh"
#include "timers.hh"
#include <interrupts/apic.hh>
#include <types.hh>
#include <x86_asm.hh>
#include <kern_logger/kern_logger.hh>

extern bool have_invariant_tsc;
extern u64 boot_tsc;
extern bool have_tsc_deadline;

using namespace kernel::x86;

namespace kernel::x86::tsc {

struct TscSource final : time::TimeSource {
    virtual u64 get_absolute_time() const override;
    virtual const char *name() const override;
    virtual void init_as_main() override;
};

TscSource tscc;

u64 TscSource::get_absolute_time() const {
    u64 tsc = rdtsc();
    return interrupts::lapic::tsc_inverted_freq * tsc;
}

const char *TscSource::name() const
{
    return "TSC";
}

void init_tsc()
{
    if (!have_invariant_tsc)
        return;

    time::kernel_timesource = &tscc;
}


bool use_tsc_deadline()
{
    return have_tsc_deadline and time::kernel_timesource == &tscc;
}


void TscSource::init_as_main()
{
    if (use_tsc_deadline()) {
        log::serial_logger.printf("Using TSC deadline!\n");
        interrupts::lapic::init_tsc_deadline();
    }
}

}