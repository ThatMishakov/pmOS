#include <uacpi/tables.h>
#include <uacpi/acpi.h>
#include <kern_logger/kern_logger.hh>
#include <memory/paging.hh>
#include <memory/vmm.hh>
#include <pmos/utility/scope_guard.hh>
#include <pmos/io.h>
#include "timers.hh"
#include <x86_utils.hh>

using namespace kernel;
using namespace kernel::x86::time;

namespace kernel::x86::hpet {

const char *HPET_NAME = "HPET";

FreqFraction hpet_freq;
FreqFraction hpet_freq_inv;

constexpr unsigned HPET_CAP_ID_REG  = 0x000;
constexpr unsigned HPET_CONF_REG    = 0x010;
constexpr unsigned HPET_COUNTER_REG = 0x0f0;
constexpr unsigned HPET_TIMER0_REG  = 0x100;

u64 phys_addr = 0;
u16 min_clock_tick = 0;
unsigned number = 0;

bool is_64bit = false;
u32 counter_clk_period;
u8 revision = 0;
unsigned num_timers = 0;

void *virt_addr;

constexpr size_t hpet_size = 1024;

bool hpet_is_always_running = false;

static u32 hpet_readl_offset(unsigned offset)
{
    return mmio_readl((u32 *)((char *)virt_addr + offset));

}
static void hpet_writel_offset(unsigned offset, u32 value)
{
    return mmio_writel((u32 *)((char *)virt_addr + offset), value);
}

static u64 hpet_read64_general(unsigned offset)
{
    u64 val = hpet_readl_offset(offset);
    val |= (u64)hpet_readl_offset(offset + 4) << 32;
    return val;
}

static void hpet_write64_general(unsigned offset, u64 val)
{
    hpet_writel_offset(offset, (u32)val);
    hpet_writel_offset(offset + sizeof(u32), val >> 32);
}

static u32 hpet_read_counter_32()
{
    return hpet_readl_offset(HPET_COUNTER_REG);
}

static u64 hpet_read_counter_64()
{
    // The spec says that 64 bit accesses might be split in 2, so do it as 2x32 bit reads on amd64 as well
    u32 low, high;
    do {
        high = hpet_readl_offset(HPET_COUNTER_REG + sizeof(u32));
        low = hpet_readl_offset(HPET_COUNTER_REG);
    } while (high != hpet_readl_offset(HPET_COUNTER_REG + sizeof(u32)));
    return ((u64)high << 32) | low;
}


static u64 timer_last_read = 0;
constexpr u64 hpet_32bit_mask = 0xffffffff;

// Read the time, emulating 64 bit HPET on 32 bit systems...
u64 hpet_read_time()
{
    if (is_64bit)
        return hpet_read_counter_64();

    u64 last_read = __atomic_load_n(&timer_last_read, __ATOMIC_RELAXED);
    u64 value = hpet_read_counter_32();
    value |= last_read & ~hpet_32bit_mask;
    if (value < last_read) {
        value += hpet_32bit_mask + 1;
    }

    if (last_read - value > (hpet_32bit_mask >> 1)) {
        __atomic_compare_exchange_n(&timer_last_read, &timer_last_read, value, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    }

    return value;
}

struct HpetSource final: TimeSource {
    virtual u64 get_absolute_time() const override;
    virtual void init_as_main() override;
    virtual const char *name() const override;
};

struct HpetCalSource final: CalibrationSource {
    virtual void prepare_for_calibration() override;
    virtual u64 wait_for_nanoseconds(u64 time_nanoseconds) const override;
    virtual const char *name() const override;
    virtual void end_calibration() override;
};

HpetSource hpet_source;
HpetCalSource hpet_cal_source;

u64 HpetSource::get_absolute_time() const
{
    return hpet_freq_inv * hpet_read_time();
}

const char *HpetSource::name() const
{
    return HPET_NAME;
} 

const char *HpetCalSource::name() const
{
    return HPET_NAME;
}

struct Runner: sched::TimerNode {
    void fire();
};

void Runner::fire()
{
    auto c = sched::get_cpu_struct();

    hpet_read_time();

    fire_at_ns += 1'000'000'000;

    c->timer_queue.insert(this);
    sched::maybe_rearm_timer(fire_at_ns);
}

void HpetSource::init_as_main()
{
    // Start the counter
    u64 reg = hpet_read64_general(HPET_CONF_REG);
    reg |= 0x01;
    hpet_write64_general(HPET_CONF_REG, reg);

    // Don't bother with overflows with 64 bit hpet...
    if (!is_64bit) {
        auto c = sched::get_cpu_struct();

        auto runner = new Runner();
        if (!runner)
            panic("Failed to allocate memory for the kernel!");
        
        // Same deal as with the ACPI PM timer, run this periodically to check for HPET rollovers...
        runner->fire_at_ns = 1'000'000'000;
        c->timer_queue.insert(runner);
    }

    hpet_is_always_running = true;
}

void HpetCalSource::prepare_for_calibration()
{
    if (!hpet_is_always_running) {
        // Start HPET
        u64 reg = hpet_read64_general(HPET_CONF_REG);
        reg |= 0x01;
        hpet_write64_general(HPET_CONF_REG, reg);
    }
}

void HpetCalSource::end_calibration()
{
    if (!hpet_is_always_running) {
        // Stop HPET...
        u64 reg = hpet_read64_general(HPET_CONF_REG);
        reg &= ~(u64)0x01;
        hpet_write64_general(HPET_CONF_REG, reg);
    }  
}

u64 HpetCalSource::wait_for_nanoseconds(u64 ns) const
{
    u64 ticks = hpet_freq * ns;
    u64 time = hpet_read_time();
    u64 init = time;
    u64 end = time + ticks;
    do {
        x86_pause();
        time = hpet_read_time();
    } while (time < end);
    return hpet_freq_inv * (time - init);
}

void init_hpet()
{
    struct uacpi_table table;
    auto ret = uacpi_table_find_by_signature(ACPI_HPET_SIGNATURE, &table);
    if (uacpi_unlikely_error(ret)) {
        log::serial_logger.printf("Couldn't get HPET table: %s", uacpi_status_to_string(ret));
        return;
    }

    acpi_hpet *hpet_table = (acpi_hpet *)table.ptr;

    u64 address = hpet_table->address.address;
    auto period  = hpet_table->min_clock_tick;
    unsigned nnumber = hpet_table->number;

    uacpi_table_unref(&table);

    log::serial_logger.printf("Found HPET! Address 0x%lx, period %i, number %i\n", address, period, nnumber);
    log::global_logger.printf("Found HPET! Address 0x%lx, period %i, number %i\n", address, period, nnumber);

    phys_addr = address;
    min_clock_tick = period;
    number = nnumber;

    constexpr u64 page_mask = PAGE_SIZE - 1;
    u64 addr_start = address & ~page_mask;
    u64 addr_end = (address + hpet_size + page_mask) & ~page_mask;

    size_t size_pages = (addr_end - addr_start) / PAGE_SIZE;
    void *mapping = vmm::kernel_space_allocator.virtmem_alloc(size_pages);
    if (!mapping)
        panic("Failed to alloc virt space for HPET!\n");
    auto guard = pmos::utility::make_scope_guard([=]() {
        vmm::kernel_space_allocator.virtmem_free(mapping, size_pages);
    });

    static const paging::Page_Table_Arguments arg = {
        .readable = true,
        .writeable = true,
        .user_access = false,
        .global = false,
        .execution_disabled = true,
        .extra = PAGE_SPECIAL,
        .cache_policy = paging::Memory_Type::IONoCache,
    };
    auto result = paging::map_kernel_pages(addr_start, mapping, addr_end - addr_start, arg);
    if (result) {
        log::serial_logger.printf("Failed to map HPET!\n");
        return;
    }
    guard.dismiss();

    virt_addr = (void *)((char *)mapping + (address & page_mask));

    auto cap = hpet_read64_general(HPET_CAP_ID_REG);
    
    counter_clk_period = cap >> 32;
    u16 vendor_id = u16(cap >> 16);
    // bool leg_rt_cap = cap & (1 << 15);
    is_64bit = cap & (1 << 13);
    num_timers = (unsigned(cap >> 8) & 0x1f) + 1;
    revision = (u8)cap;

    if (revision == 0) {
        log::serial_logger.printf("HPET Revision 0! Ignoring it!\n");
        return;
    }

    log::serial_logger.printf("HPET revision: %u, number of timers: %u, 64 bit: %s, vendor id: %u, clock period: %u\n", revision, num_timers, is_64bit ? "true" : "false", vendor_id, counter_clk_period);

    // HPET's clock period is in femptoseconds, the kernel stores time in nanoseconds, so 1e15/1e9
    hpet_freq = computeFreqFraction(1e6, counter_clk_period);
    hpet_freq_inv = computeFreqFraction(counter_clk_period, 1e6);

    // Disable HPET (just in case)
    u64 reg = hpet_read64_general(HPET_CONF_REG);
    reg &= ~(u64)0x03;
    hpet_write64_general(HPET_CONF_REG, reg);

    // Set counter to zero, for later
    hpet_writel_offset(HPET_COUNTER_REG, 0);
    if (is_64bit)
        hpet_writel_offset(HPET_COUNTER_REG + sizeof(u32), 0);

    // Disable timers...
    for (unsigned i = 0; i < num_timers; ++i) {
        auto offset = HPET_TIMER0_REG + i*0x20;
        auto val = hpet_read64_general(offset);
        val &= ~(u64)(1 << 2); // Unset interrupt enable
        hpet_write64_general(offset, val);
    }

    kernel_timesource = &hpet_source;
    kernel_calibration_source = &hpet_cal_source;
}

}
