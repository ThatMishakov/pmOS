#include "tsc.hh"
#include "timers.hh"
#include <interrupts/apic.hh>
#include <types.hh>
#include <x86_asm.hh>
#include <x86_utils.hh>
#include <kern_logger/kern_logger.hh>

extern bool have_invariant_tsc;
extern u64 boot_tsc;
extern bool have_tsc_deadline;


using namespace kernel::x86;

namespace kernel::x86::tsc {

FreqFraction tsc_freq;
FreqFraction tsc_inverted_freq;

struct TscSource final : time::TimeSource {
    virtual u64 get_absolute_time() const override;
    virtual const char *name() const override;
    virtual void init_as_main() override;
};

TscSource tscc;

u64 TscSource::get_absolute_time() const {
    u64 tsc = rdtsc();
    return tsc_inverted_freq * tsc;
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

bool tsc_calibrated = false;

bool calibrate_tsc_cpuid()
{
    u32 max_leaf = 0;
    auto c = cpuid(0x0);
    max_leaf = c.eax;

    if (max_leaf >= 0x15) {
        auto c = cpuid(0x015);
        if (c.ecx) {
            // Use u64 for implicit upcasts :P
            u64 freq = c.ecx;
            u64 denominator = c.eax;
            u64 numerator = c.ebx;

            // Nanoseconds for the total confusion...
            tsc_freq = computeFreqFraction(freq * numerator, 1e9 * denominator);
            tsc_inverted_freq = computeFreqFraction(1e9 * denominator, freq * numerator);

            tsc_calibrated = true;

            return true;
        }
    }

    return false;
}

void calibrate_tsc()
{
    if (tsc_calibrated)
        return;

    if (calibrate_tsc_cpuid()) {
        log::serial_logger.printf("Kernel: TSC frequency is known from CPUID: %li\n", tsc_freq * 1'000'000'000);
        return;
    }

    if (!time::kernel_calibration_source)
        // If TSC was chosen and can't be calibrated, then hpet, lapic and other clocks are not usable either
        panic("No calibration source for TSC!");

    log::serial_logger.printf("Calibrating TSC with %s...\n", time::kernel_calibration_source->name());

    constexpr u64 cal_time = 10'000'000; // 10ms

    time::kernel_calibration_source->prepare_for_calibration();
    u64 tsc_start = rdtsc();
    u64 actual_time = time::kernel_calibration_source->wait_for_nanoseconds(cal_time);
    u64 tsc_end = rdtsc();

    tsc_freq          = computeFreqFraction(tsc_end - tsc_start, actual_time);
    tsc_inverted_freq = computeFreqFraction(actual_time, tsc_end - tsc_start);

    tsc_calibrated = true;

    log::global_logger.printf("[Kernel] Info: TSC frequency: %li, calibrated with %s\n", tsc_freq * 1'000'000'000, time::kernel_calibration_source->name());
    log::serial_logger.printf("[Kernel] Info: TSC ticks per 1ms: %lu, calibrated over %lu ns\n", tsc_freq * 1'000'000, actual_time);
}

void TscSource::init_as_main()
{
    calibrate_tsc();

    if (use_tsc_deadline()) {
        log::serial_logger.printf("Using TSC deadline!\n");
        interrupts::lapic::init_tsc_deadline();
    }
}

}