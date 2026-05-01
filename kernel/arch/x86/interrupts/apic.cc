/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "apic.hh"

#include "ioapic.hh"
#include "pic.hh"
#include "pit.hh"

#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/paging.hh>
#include <memory/vmm.hh>
#include <pmos/ipc.h>
#include <sched/sched.hh>
#include <utils.hh>
#include <x86_asm.hh>
#include <x86_utils.hh>
#include <pmos/io.h>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <time/tsc.hh>
#include <time/timers.hh>

using namespace kernel;
using namespace kernel::x86;
using namespace kernel::x86::interrupts::lapic;
using namespace kernel::x86::interrupts;
using namespace kernel::log;

bool have_invariant_tsc = false;
bool have_tsc_deadline  = false;
u64 boot_tsc            = 0;

namespace kernel::x86::interrupts::lapic {

u64 apic_base = 0xFEE00000;

static void *apic_mapped_addr = nullptr;

void map_apic()
{
    uacpi_table t;
    auto res = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &t);
    if (res != UACPI_STATUS_OK) {
        serial_logger.printf("Couldn't get MADT when mapping apic (%s)! Using address from IA32_APIC_BASE_MSR...\n", uacpi_status_to_string(res));

        auto val = read_msr(IA32_APIC_BASE_MSR);
        if (val & (1 << 11) && !(val & (1 << 10)))
            apic_base = val & ~(u64)0xfff;
        else
            serial_logger.printf("Using hardcoded LAPIC address!\n");
    } else {
        acpi_madt *madt = (acpi_madt *)t.ptr;

        apic_base = static_cast<u64>(madt->local_interrupt_controller_address);

        size_t offset = sizeof(acpi_madt);
        while (offset < madt->hdr.length) {
            acpi_entry_hdr *ee = (acpi_entry_hdr *)((char *)madt + offset);
            offset += ee->length;
            assert(ee->length);
            if (ee->type != ACPI_MADT_ENTRY_TYPE_LAPIC_ADDRESS_OVERRIDE)
                continue;

            acpi_madt_lapic_address_override *e = (acpi_madt_lapic_address_override *)ee;
            apic_base = e->address;
            break;
        }

        uacpi_table_unref(&t);
    }

    serial_logger.printf("LAPIC base: %lx\n", apic_base);

    apic_mapped_addr = vmm::kernel_space_allocator.virtmem_alloc(1);
    if (!apic_mapped_addr)
        panic("Couldn't allocate virtual memory for LAPIC!!\n");

    paging::Page_Table_Arguments arg = {
        .readable = true,
        .writeable = true,
        .user_access = false,
        .global = true,
        .execution_disabled = true,
        .extra = PAGE_SPECIAL,
        .cache_policy = paging::Memory_Type::IONoCache,
    };

    auto result = paging::map_kernel_page(apic_base, apic_mapped_addr, arg);
    if (result)
        panic("Failed to map LAPIC!!");
}

APICMode apic_mode = APICMode::XAPIC;
bool can_use_32bit_apic_ids = false;


static bool cant_diable_x2apic()
{
    // TODO: Check MAX leaf

    auto cc = cpuid(0x07);
    if (!(cc.edx & (1  << 29)))
        return false;

    auto c = read_msr(IA32_ARCH_CAPABILITIES_MSR);
    if (!(c & (1 << 21)))
        return false;

    c = read_msr(A32_XAPIC_DISABLE_STATUS_MSR);
    if (c & 0x01)
        return true;
    return false;
}

static void detect_x2apic()
{
    auto c = cpuid(0x01);
    if (c.ecx & (1 << 21)) {
        serial_logger.printf("System has X2APIC!\n");
        apic_mode = APICMode::X2APIC;
        can_use_32bit_apic_ids = true;
    } else {
        return;
    }

    uacpi_table t;
    auto res = uacpi_table_find_by_signature(ACPI_DMAR_SIGNATURE, &t);
    if (res == UACPI_STATUS_OK) {
        auto dmar = (acpi_dmar *)t.ptr;

        if (dmar->flags & ACPI_DMAR_X2APIC_OPT_OUT) {
            serial_logger.printf("Firmware doesn't want us to use x2APIC...\n");

            if (!cant_diable_x2apic()) {
                apic_mode = APICMode::XAPIC;
            }
            can_use_32bit_apic_ids = false;
        }

        uacpi_table_unref(&t);
    }
}

static void enable_x2apic()
{
    auto r = read_msr(IA32_APIC_BASE_MSR);
    if (!(r & (1 << 10))) {
        r |= 1 << 10;
        write_msr(IA32_APIC_BASE_MSR, r);
    }
}

void enable_apic()
{
    if (apic_mode == APICMode::X2APIC)
        enable_x2apic();
    
    if (apic_mode != APICMode::X2APIC) {
        auto state = read_msr(IA32_APIC_BASE_MSR);
        // auto bsp_flag = state & (1 << 8);

        if (state & (1 << 10)) // x2APIC enabled. Disable it, and re-enable LAPIC in xAPIC mode
            write_msr(IA32_APIC_BASE_MSR, 0);

        write_msr(IA32_APIC_BASE_MSR, apic_base | (state & (u64)0xbff) | (1 << 11));
    
        // This doesn't exist with X2APIC
        apic_write_reg(APIC_REG_DFR, 0xffffffff);
    }
     
    // apic_write_reg(APIC_REG_LDR, 0x01000000);
    apic_write_reg(APIC_REG_LVT_TMR, APIC_LVT_MASK);
    apic_write_reg(APIC_REG_LVT_INT0, LVT_INT0);
    apic_write_reg(APIC_REG_LVT_INT1, LVT_INT1);
    apic_write_reg(APIC_REG_SPURIOUS_INT, APIC_SPURIOUS_INT | 0x100);

    apic_write_reg(APIC_REG_TMRDIV, 0x3);           // Divide by 16
    apic_write_reg(APIC_REG_TMRINITCNT, 0);
    if (!tsc::use_tsc_deadline())
        apic_write_reg(APIC_REG_LVT_TMR, APIC_TMR_INT); // Init in one-shot mode
    else
        apic_write_reg(APIC_REG_LVT_TMR, APIC_TMR_INT | (1 << 18));
}

void init_tsc_deadline()
{
    apic_write_reg(APIC_REG_LVT_TMR, APIC_TMR_INT | (1 << 18));
    // Just in case the system is in xAPIC mode...
    __sync_synchronize();
}

FreqFraction apic_freq;
FreqFraction apic_inverted_freq;

static void check_tsc()
{
    auto c = cpuid(0x80000007);
    serial_logger.printf("[Kernel] Info: CPUID 0x80000007: %h %h %h %h\n", c.eax, c.ebx, c.ecx,
                         c.edx);
    if (c.edx & (1 << 8)) {
        global_logger.printf("[Kernel] Info: TSC Invariant bit is set\n");
        serial_logger.printf("[Kernel] Info: TSC Invariant bit is set\n");
        have_invariant_tsc = true;
    } else {
        global_logger.printf("[Kernel] Warning: TSC Invariant bit is not set\n");
        serial_logger.printf("[Kernel] Warning: TSC Invariant bit is not set\n");
    }
}

static void detect_tsc_deadline()
{
    auto c = cpuid(0x01);
    if (c.ecx & (1 << 24)) {
        global_logger.printf("[Kernel] Info: TSC deadline support detected...\n");
        have_tsc_deadline = true;
    }
}

void prepare_apic_bsp()
{
    init_PIC();
    detect_x2apic();
    check_tsc();
    detect_tsc_deadline();

    if (apic_mode == APICMode::XAPIC)
        map_apic();
}

bool lapic_freq_from_cpuid()
{
    constexpr u32 divisor = 16;

    u32 max_leaf = 0;
    auto c = cpuid(0x0);
    max_leaf = c.eax;

    if (max_leaf >= 0x15) {
        auto c = cpuid(0x015);
        if (c.ecx) {
            u32 clock = c.ecx;
            apic_freq = computeFreqFraction(clock, 1e9 * divisor);
            apic_inverted_freq = computeFreqFraction(1e9 * divisor, clock);
            return true;
        }
    }

    if (max_leaf >= 0x16) {
        auto c = cpuid(0x16);
        u16 bus_freq_mhz = c.ecx & 0xffff;
        if (bus_freq_mhz) {
            apic_freq = computeFreqFraction(bus_freq_mhz, 1e3 * divisor);
            apic_inverted_freq = computeFreqFraction(1e3 * divisor, bus_freq_mhz);
            return true;
        }
    }

    return false;
}

void discover_apic_freq()
{
    if (tsc::use_tsc_deadline()) {
        log::serial_logger.printf("Kernel: Using tsc deadline in kernel, so not bothering with calibrating LAPIC timer...\n");
        log::global_logger.printf("[Kernel] Info: Not using LAPIC timer\n");
        return;
    }

    if (lapic_freq_from_cpuid()) {
        auto l = apic_freq * 1'000'000'000;
        global_logger.printf("[Kernel] Info: APIC timer freq from CPUID: %li\n", l);
        serial_logger.printf("[Kernel] Info: APIC timer freq from CPUID: %li\n", l);
        return;
    }

    // TODO: APIC timer has a flag indicating it stops in C states on some system, use HPET instead in those cases...
    // (but this was not yet implemented by the kernel, so don't bother...)

    if (!time::kernel_calibration_source)
        panic("No calibration source for LAPIC timer!");

    // Enable APIC Timer and map to dummy ISR
    apic_write_reg(APIC_REG_LVT_TMR, APIC_DUMMY_ISR);

    // Set up divide value to 16
    apic_write_reg(APIC_REG_TMRDIV, 0x03);

    time::kernel_calibration_source->prepare_for_calibration();
    // Reset APIC counter
    apic_write_reg(APIC_REG_TMRINITCNT, (u32)-1);

    serial_logger.printf("[Kernel] Calibrating LAPIC with %s...\n", time::kernel_calibration_source->name());

    constexpr u64 wait_time_ns = 10'000'000; // 10ms
    u64 actual_time = time::kernel_calibration_source->wait_for_nanoseconds(wait_time_ns);

    // Get how many ticks have passed
    u32 ticks   = -apic_read_reg(APIC_REG_TMRCURRCNT);
    u64 divisor = actual_time;

    // Stop APIC timer
    apic_write_reg(APIC_REG_TMRINITCNT, 0);
    apic_write_reg(APIC_REG_LVT_TMR, APIC_LVT_MASK);

    // frequency is in nHz
    apic_freq          = computeFreqFraction(ticks, divisor);
    apic_inverted_freq = computeFreqFraction(divisor, ticks);
    auto l             = apic_freq * 1'000'000'000;
    global_logger.printf("[Kernel] Info: APIC timer frequency: %li\n", l);
    serial_logger.printf("[Kernel] Info: APIC timer ticks per 1ms: %lu, cal time: %lu ns\n", l / 1000, actual_time);
}

void arm_tsc_deadline(u64 time)
{
    write_msr(IA32_TSC_DEADLINE_MSR, time);
}

void apic_one_shot(u32 ms)
{
    u64 time_nanoseconds = ms * 1'000'000;
    u32 ticks            = apic_freq * time_nanoseconds;
    apic_write_reg(APIC_REG_TMRDIV, 0x3);           // Divide by 16
    apic_write_reg(APIC_REG_LVT_TMR, APIC_TMR_INT); // Init in one-shot mode
    apic_write_reg(APIC_REG_TMRINITCNT, ticks);
}

void apic_one_shot_ticks(u32 ticks)
{
    apic_write_reg(APIC_REG_TMRDIV, 0x3);           // Divide by 1
    apic_write_reg(APIC_REG_TMRINITCNT, ticks);
}

u64 cpu_get_apic_base() { return read_msr(IA32_APIC_BASE_MSR); }

void cpu_set_apic_base(u64 base)
{
    return write_msr(IA32_APIC_BASE_MSR, base);
}

void apic_write_reg(u16 index, u32 val)
{
    if (apic_mode == APICMode::X2APIC) {
        write_msr(X2APIC_MSR_BASE + index, val);
    } else {
        u32 *reg = (u32 *)((char *)apic_mapped_addr + index * 0x10);
        mmio_writel(reg, val);
    }
}

u32 apic_read_reg(u16 index)
{
    if (apic_mode == APICMode::X2APIC) {
        // For 64 bit regs just do raw write I guess?
        return (u32)read_msr(X2APIC_MSR_BASE + index);
    } else {
        u32 *reg = (u32 *)((char *)apic_mapped_addr + (index * 0x10));
        return mmio_readl(reg);
    } 
}

void apic_eoi() { apic_write_reg(APIC_REG_EOI, 0); }

u32 apic_get_remaining_ticks()
{
    return apic_read_reg(APIC_REG_TMRCURRCNT);
}

// u64 lapic_configure(u64 opt, u64 arg)
// {
//     u64 result = {0};

//     switch (opt) {
//     case 0:
//         result = get_lapic_id();
//         break;
//     case 1:
//         broadcast_init_ipi();
//         break;
//     case 2:
//         broadcast_sipi(arg);
//         break;
//     case 3:
//         send_ipi_fixed(arg >> 8, arg);
//         break;
//     default:
//         throw(Kern_Exception(ENOSYS, "lapic_configure with unsupported parameter\n"));
//     };
//     return result;
// }

u32 get_lapic_id() {
    auto id = apic_read_reg(APIC_REG_LAPIC_ID);
    if (apic_mode == APICMode::X2APIC)
        return id;
    else
        return id >> 24;
}

void apic_write_icr(u64 dest)
{
    if (apic_mode != APICMode::X2APIC) {
        apic_write_reg(APIC_ICR_HIGH, static_cast<u32>(dest >> 32));
        apic_write_reg(APIC_ICR_LOW, static_cast<u32>(dest));
    } else {
        write_msr(APIC_ICR_LOW + X2APIC_MSR_BASE, dest);
    }
}

void broadcast_sipi(u8 vector)
{
    apic_write_icr(0x000C4600 | vector);
}

void broadcast_init_ipi()
{
    apic_write_icr(0x000C4500);
}

void send_ipi_fixed(u8 vector, u32 dest)
{
    u64 dest_shifted = apic_mode == APICMode::X2APIC ? (u64)dest << 32 : (u64)dest << (32 + 24);
    u64 val = dest_shifted | (u32)vector | (0x01 << 14);
    apic_write_icr(val);

    // serial_logger.printf("[Kernel] Info: Sending IPI to %h with vector %h\n", dest, vector);

}

void send_init_ipi(u32 dest)
{
    u64 dest_shifted = apic_mode == APICMode::X2APIC ? (u64)dest << 32 : (u64)dest << (32 + 24);
    u64 val = dest_shifted | (0b101 << 8) | (1 << 14);
    apic_write_icr(val);
}

void send_sipi(u8 vector, u32 dest)
{
    u64 dest_shifted = apic_mode == APICMode::X2APIC ? (u64)dest << 32 : (u64)dest << (32 + 24);
    u64 val = dest_shifted | (0b110 << 8) | (1 << 14) | vector;
    apic_write_icr(val);
}

void send_ipi_fixed_others(u8 vector)
{
    apic_write_icr(vector | (0x01 << 14) | (0b11 << 18));
}

static void smart_eoi(u8 intno)
{
    u8 isr_index = intno >> 5;
    u8 offset    = intno & 0x1f;

    u32 isr_val = apic_read_reg(APIC_ISR_REG_START + isr_index);

    if (isr_val & (0x01 << offset))
        apic_eoi();
}

}

void lvt0_int_routine()
{
    serial_logger.printf("[Kernel] Info: LVT0 interrupt\n");
    smart_eoi(LVT_INT0);
}

void lvt1_int_routine()
{
    serial_logger.printf("[Kernel] Info: LVT1 interrupt\n");
    smart_eoi(LVT_INT1);
}

void apic_spurious_int_routine()
{
    global_logger.printf("[Kernel] Info: Spurious LAPIC interrupt\n");
    serial_logger.printf("[Kernel] Info: Spurious LAPIC interrupt\n");
}

void apic_dummy_int_routine() { smart_eoi(APIC_DUMMY_ISR); }

void lapic::tpr_write(unsigned val)
{
    assert(val < 16);
#ifdef __x86_64__
    setCR8(val);
#else
    apic_write_reg(APIC_REG_TPR, val << 4);
#endif
}

unsigned lapic::tpr_read()
{
#ifdef __x86_64__
    return getCR8();
#else
    return (apic_read_reg(APIC_REG_TPR) >> 4) & 0xff;
#endif
}

extern "C" void programmable_interrupt(u32 intno)
{
    auto c = sched::get_cpu_struct();

    auto offset = intno - first_mappable_vector;
    auto handler = __atomic_load_n(&c->isr_handlers[offset], __ATOMIC_RELAXED);
    if (!handler) {
        global_logger.printf("[Kernel] Error: No handler for interrupt %h\n", intno);
        apic_eoi();
        tpr_write(0);
        // panic?? this can't really be handled without iommu
        return;
    }

    if (handler->send_interrupt_notification() != kernel::interrupts::NotificationResult::Success) {
        global_logger.printf("[Kernel] Warning: Failed to send interrupt notification for interrupt %h\n", intno);

        // TODO if I end up sopporting MSIs
        auto ioapic_handler = static_cast<IOAPIC_Handler *>(handler);
        ioapic_handler->mask_interrupt();
        apic_eoi();
        tpr_write(0);
        return;
    } else {
        tpr_write(14);
    }
}

void ::kernel::interrupts::interrupt_complete(InterruptHandler *handler)
{
    assert(handler);
    
    // Everything goes through IOAPIC for now anyway...
    auto ioapic_handler = static_cast<IOAPIC_Handler *>(handler);

    assert(ioapic_handler->lapic_vector >= first_mappable_vector);
    u32 intno = ioapic_handler->lapic_vector;
    
    assert(intno < 256);
    smart_eoi(intno);
    tpr_write(0);
}

void lapic::lapic_timer_set_dummy()
{
    if (!tsc::use_tsc_deadline())
        apic_write_reg(APIC_REG_LVT_TMR, APIC_DUMMY_ISR); // Init in one-shot mode
    else
        apic_write_reg(APIC_REG_LVT_TMR, APIC_DUMMY_ISR | (1 << 18));
}

void lapic::lapic_timer_restore_normal()
{
    if (!tsc::use_tsc_deadline())
        apic_write_reg(APIC_REG_LVT_TMR, APIC_TMR_INT); // Init in one-shot mode
    else
        apic_write_reg(APIC_REG_LVT_TMR, APIC_TMR_INT | (1 << 18));
}