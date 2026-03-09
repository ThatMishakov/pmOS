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

using namespace kernel;
using namespace kernel::x86;
using namespace kernel::x86::interrupts::lapic;
using namespace kernel::x86::interrupts;
using namespace kernel::log;

bool have_invariant_tsc = false;
u64 boot_tsc            = 0;

namespace kernel::x86::interrupts::lapic {

u64 apic_base = 0xFEE00000;

static void *apic_mapped_addr = nullptr;

void map_apic()
{
    uacpi_table t;
    auto res = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &t);
    if (res != UACPI_STATUS_OK) {
        serial_logger.printf("Couldn't get MADT when mapping apic! Using address from IA32_APIC_BASE_MSR...\n");

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
}

void prepare_apic_bsp()
{
    init_PIC();
    detect_x2apic();

    if (apic_mode == APICMode::XAPIC)
        map_apic();
}

FreqFraction apic_freq;
FreqFraction tsc_freq;

FreqFraction apic_inverted_freq;
FreqFraction tsc_inverted_freq;

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

void discover_apic_freq()
{
    check_tsc();
    // While we're here, also measure TSC frequency
    u64 tsc_start;

    // Enable APIC Timer and map to dummy ISR
    apic_write_reg(APIC_REG_LVT_TMR, APIC_DUMMY_ISR);

    // Set up divide value to 16
    apic_write_reg(APIC_REG_TMRDIV, 0x03);

    // Play with keyboard controller since channel 2 timer is
    // connected to it... (I don't undertstand this)
    u8 p = inb(0x61);
    p    = (p & 0xfd) | 1;
    outb(0x61, p);

    outb(PIT_MODE_REG, 0b10'11'000'0);
    set_pit_count(0x2e9b, 2);

    // Start timer 2
    p = inb(0x61);
    p = (p & 0xfe);
    outb(0x61, p);     // Gate LOW
    outb(0x61, p | 1); // Gate HIGH

    tsc_start = rdtsc();
    // Reset APIC counter
    apic_write_reg(APIC_REG_TMRINITCNT, (u32)-1);

    // Wait for PIT timer 2 to reach 0
    while (not(inb(0x61) & 0x20))
        ;

    u64 tsc_end = rdtsc();

    // Get how many ticks have passed
    u32 ticks   = -apic_read_reg(APIC_REG_TMRCURRCNT);
    u64 divisor = 10'000'000; // 10ms

    // Stop APIC timer
    apic_write_reg(APIC_REG_TMRINITCNT, 0);
    apic_write_reg(APIC_REG_LVT_TMR, APIC_LVT_MASK);

    // frequency is in nHz
    apic_freq          = computeFreqFraction(ticks, divisor);
    apic_inverted_freq = computeFreqFraction(divisor, ticks);
    auto l             = apic_freq * 1'000'000'000;
    global_logger.printf("[Kernel] Info: APIC timer ticks per 1ms: %li\n", l);
    serial_logger.printf("[Kernel] Info: APIC timer ticks per 1ms: %lu %u\n", l, ticks * 100);

    tsc_freq          = computeFreqFraction(tsc_end - tsc_start, divisor);
    tsc_inverted_freq = computeFreqFraction(divisor, tsc_end - tsc_start);
    global_logger.printf("[Kernel] Info: TSC ticks per 1ms: %li\n", tsc_freq * 1'000'000'000);
    serial_logger.printf("[Kernel] Info: TSC ticks per 1s: %li %li\n", tsc_freq * 1'000'000'000,
                         (tsc_end - tsc_start) * 100);
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
    apic_write_reg(APIC_REG_LVT_TMR, APIC_TMR_INT); // Init in one-shot mode
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

u32 get_lapic_id() { return apic_read_reg(APIC_REG_LAPIC_ID); }

static void apic_write_icr(u64 dest)
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
    u64 val = (((u64)dest << 32) | (u32)vector | (0x01 << 14));
    apic_write_icr(val);

    // serial_logger.printf("[Kernel] Info: Sending IPI to %h with vector %h\n", dest, vector);

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

ReturnStr<std::pair<sched::CPU_Info *, u32>> allocate_interrupt(IntMapping m)
{
    // Find least loaded CPU
    auto *cpu = sched::cpus[0];
    for (size_t i = 1; i < sched::cpus.size(); ++i) {
        if (sched::cpus[i]->int_handlers.allocated_int_count <
            cpu->int_handlers.allocated_int_count)
            cpu = sched::cpus[i];
    }

    // Find unused slot
    u32 idx = 0;
    for (; idx < cpu->MAPPABLE_INTS; ++idx) {
        if (!cpu->int_mappings[idx].first)
            break;
    }

    if (idx == cpu->MAPPABLE_INTS)
        return Error(-ENOMEM);

    cpu->int_mappings[idx].first  = m.ioapic;
    cpu->int_mappings[idx].second = m.vector;

    cpu->int_handlers.allocated_int_count++;

    return Success(std::make_pair(cpu, idx + 48));
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

void tpr_write(unsigned val)
{
#ifdef __x86_64__
    setCR8(val);
#else
    apic_write_reg(APIC_REG_TPR, val << 4);
#endif
}

void kernel::interrupts::interrupt_complete(u32 intno)
{
    assert(intno < 256);
    smart_eoi(intno);
    tpr_write(0);
}

u32 kernel::interrupts::interrupt_min() { return 48; }
u32 kernel::interrupts::interrupt_limint() { return 240; }

extern "C" void programmable_interrupt(u32 intno)
{
    auto c = sched::get_cpu_struct();

    auto handler = c->int_handlers.get_handler(intno);
    if (!handler) {
        global_logger.printf("[Kernel] Error: No handler for interrupt %h\n", intno);
        apic_eoi();
        return;
    }

    auto port = handler->port;
    bool sent = false;
    if (port) {
        IPC_Kernel_Interrupt kmsg = {IPC_Kernel_Interrupt_NUM, intno, c->cpu_id};
        auto r = port->atomic_send_from_system(reinterpret_cast<char *>(&kmsg), sizeof(kmsg));
        if (r < 0)
            global_logger.printf(
                "[Kernel] Error: Could not send interrupt %h to port %h error %i\n", intno,
                port->portno, r);
        else
            sent = true;
    }

    if (not sent) {
        apic_eoi();

        c->int_handlers.remove_handler(intno);
    } else {
        handler->active = true;
        // Disable reception of other interrupts from peripheral devices until this one is
        // handled
        tpr_write(14);
    }
}

extern Spinlock int_allocation_lock;

kresult_t kernel::interrupts::interrupt_enable(u32 i)
{
    assert(i >= 48 and i < 240);

    i -= 48;

    x86::interrupts::IOAPIC *ioapic = nullptr;
    u32 vector                      = 0;
    auto c                          = sched::get_cpu_struct();

    {
        Auto_Lock_Scope l(int_allocation_lock);
        auto [ii, v] = c->int_mappings[i];
        ioapic       = (IOAPIC *)ii;
        vector       = v;
    }

    if (!ioapic)
        return -ENOENT;

    ioapic->interrupt_enable(vector);
    return 0;
}

void kernel::interrupts::interrupt_disable(u32 i)
{
    assert(i >= 48 and i < 240);

    x86::interrupts::IOAPIC *ioapic = nullptr;
    u32 vector                      = 0;
    auto c                          = sched::get_cpu_struct();

    {
        Auto_Lock_Scope l(int_allocation_lock);
        auto [ii, v] = c->int_mappings[i];
        ioapic       = (IOAPIC *)ii;
        vector       = v;
    }

    if (ioapic)
        ioapic->interrupt_disable(vector);
}