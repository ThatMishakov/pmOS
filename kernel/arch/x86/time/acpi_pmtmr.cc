#include "acpi_pmtmr.hh"
#include <types.hh>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <kern_logger/kern_logger.hh>
#include "timers.hh"
#include <utils.hh>
#include <x86_utils.hh>
#include <sched/sched.hh>

using namespace kernel::x86::time;

namespace kernel::x86::acpi_pmtmr {

static const FreqFraction acpi_pm_freq     = computeFreqFraction(3579545, 1e9);
static const FreqFraction acpi_pm_freq_inv = computeFreqFraction(1e9, 3579545);

static u32 pmtmr_ioport = 0;
static u32 timer_mask   = 0xffffff;

struct AcpiPmTmrSource final: TimeSource {
    virtual u64 get_absolute_time() const override;
    virtual void init_as_main() override;
    virtual const char *name() const override;
};


struct AcpiPmTmrCalSource final: CalibrationSource {
    virtual u64 wait_for_nanoseconds(u64 time_nanoseconds) const override;
    virtual const char *name() const override;
};

AcpiPmTmrSource acpi_pm_tmr_timesource = {};
AcpiPmTmrCalSource acpi_pm_tmr_calsource = {};

static u32 apci_pmtmr_read()
{
    return inl(pmtmr_ioport) & timer_mask;
}

static u64 timer_last_read = 0;

// "Inspired by" https://github.com/dbstream/davix/blob/8dad6a9c9197dbac7904e169221b127269f946b3/arch/x86/kernel/time.cc#L87-L114

static u64 read_accumulated()
{
    u64 value = apci_pmtmr_read();

    u64 last_read = __atomic_load_n(&timer_last_read, __ATOMIC_RELAXED);
    value |= last_read & ~(u64)timer_mask;
    if (value < last_read) {
        value += (u64)timer_mask + 1;
    }

    if (last_read - value > (timer_mask >> 1)) {
        __atomic_compare_exchange_n(&timer_last_read, &timer_last_read, value, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    }

    return value;
}

u64 AcpiPmTmrSource::get_absolute_time() const
{
    return acpi_pm_freq_inv * read_accumulated();
}

const char acpi_pmtmr_name[] = "ACPI PM Timer";
const char *AcpiPmTmrSource::name() const
{
    return acpi_pmtmr_name;
}

const char *AcpiPmTmrCalSource::name() const
{
    return acpi_pmtmr_name;
}

struct Runner: sched::TimerNode {
    void fire();
};

void Runner::fire()
{
    auto c = sched::get_cpu_struct();

    read_accumulated();

    fire_at_ns += 1'000'000'000;

    c->timer_queue.insert(this);
    sched::maybe_rearm_timer(fire_at_ns);
}

void AcpiPmTmrSource::init_as_main()
{
    auto c = sched::get_cpu_struct();

    auto runner = new Runner();
    if (!runner)
        panic("Failed to allocate memory for the kernel!");
    
    runner->fire_at_ns = 1'000'000'000;
    c->timer_queue.insert(runner);
}

void init_acpi_pmtmr()
{
    struct acpi_fadt *fadt;
    auto res = uacpi_table_fadt(&fadt);
    if (res != UACPI_STATUS_OK)
        return;

    if (fadt->pm_tmr_len != 4) {
        return;
    }

    if (fadt->hdr.revision >= 3) {
        if (fadt->x_pm_tmr_blk.address_space_id != ACPI_AS_ID_SYS_IO) {
            log::serial_logger.printf("ACPI PM Timer unknown address space ID in FADT: %i\n", fadt->x_pm_tmr_blk.address_space_id);
            return;
        }

        pmtmr_ioport = (u32)fadt->x_pm_tmr_blk.address;
        if (pmtmr_ioport == 0)
            pmtmr_ioport = fadt->pm_tmr_blk;
    } else {
        pmtmr_ioport = fadt->pm_tmr_blk;
    }

    u32 size = 24;
    if (fadt->flags & ACPI_TMR_VAL_EXT) {
        size = 32;
        timer_mask = 0xffffffff;
    }


    if (!pmtmr_ioport)
        return;

    log::serial_logger.printf("Found %i bit ACPI PM Timer at port 0x%x\n", size, pmtmr_ioport);
    log::global_logger.printf("Found %i bit ACPI PM Timer at port 0x%x\n", size, pmtmr_ioport);

    kernel_timesource = &acpi_pm_tmr_timesource;
    kernel_calibration_source = &acpi_pm_tmr_calsource;
}

u64 AcpiPmTmrCalSource::wait_for_nanoseconds(u64 ns) const
{
    u64 ticks = acpi_pm_freq * ns;
    u64 time = read_accumulated();
    u64 init = time;
    u64 wanted = time + ticks;
    do {
        x86_pause();
        time = read_accumulated();
    } while (time < wanted);
    return acpi_pm_freq_inv * (time - init);
}

}
