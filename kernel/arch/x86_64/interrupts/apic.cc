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

using namespace kernel;

void *apic_mapped_addr = nullptr;

void map_apic()
{
    apic_mapped_addr = vmm::kernel_space_allocator.virtmem_alloc(1);
    map_kernel_page(apic_base, apic_mapped_addr, {1, 1, 0, 0, 0, PAGE_SPECIAL});
}

void enable_apic()
{
    apic_write_reg(APIC_REG_DFR, 0xffffffff);
    apic_write_reg(APIC_REG_LDR, 0x01000000);
    apic_write_reg(APIC_REG_LVT_TMR, APIC_LVT_MASK);
    apic_write_reg(APIC_REG_LVT_INT0, LVT_INT0);
    apic_write_reg(APIC_REG_LVT_INT1, LVT_INT1);
    cpu_set_apic_base(cpu_get_apic_base());
    apic_write_reg(APIC_REG_SPURIOUS_INT, APIC_SPURIOUS_INT | 0x100);
}

void prepare_apic()
{
    init_PIC();
    map_apic();
}

FreqFraction apic_freq;
FreqFraction tsc_freq;

FreqFraction apic_inverted_freq;
FreqFraction tsc_inverted_freq;

bool have_invariant_tsc = false;
u64 boot_tsc            = 0;
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
    auto l = apic_freq*1'000'000'000;
    global_logger.printf("[Kernel] Info: APIC timer ticks per 1ms: %i\n", l);
    serial_logger.printf("[Kernel] Info: APIC timer ticks per 1ms: %i\n", l);

    tsc_freq          = computeFreqFraction(tsc_end - tsc_start, divisor);
    tsc_inverted_freq = computeFreqFraction(divisor, tsc_end - tsc_start);
    global_logger.printf("[Kernel] Info: TSC ticks per 1ms: %i\n", tsc_freq*1'000'000'000);
    serial_logger.printf("[Kernel] Info: TSC ticks per 1s: %i %i\n", tsc_freq*1'000'000'000, (tsc_end - tsc_start)*100);
}

void apic_one_shot(u32 ms)
{
    u64 time_nanoseconds = ms * 1'000'000;
    u32 ticks = apic_freq * time_nanoseconds;
    apic_write_reg(APIC_REG_TMRDIV, 0x3);           // Divide by 16
    apic_write_reg(APIC_REG_LVT_TMR, APIC_TMR_INT); // Init in one-shot mode
    apic_write_reg(APIC_REG_TMRINITCNT, ticks);
}

void apic_one_shot_ticks(u32 ticks)
{
    apic_write_reg(APIC_REG_TMRDIV, 0x3);        // Divide by 1
    apic_write_reg(APIC_REG_LVT_TMR, APIC_TMR_INT); // Init in one-shot mode
    apic_write_reg(APIC_REG_TMRINITCNT, ticks);
}

u64 cpu_get_apic_base() { return read_msr(IA32_APIC_BASE_MSR); }

void cpu_set_apic_base(u64 base) { return write_msr(IA32_APIC_BASE_MSR, base); }

void apic_write_reg(u16 index, u32 val)
{
    volatile u32 *reg = (volatile u32 *)((u64)apic_mapped_addr + index);
    *reg              = val;
}

u32 apic_read_reg(u16 index)
{
    volatile u32 *reg = (volatile u32 *)((u64)apic_mapped_addr + index);
    return *reg;
}

void apic_eoi() { apic_write_reg(APIC_REG_EOI, 0); }

u32 apic_get_remaining_ticks() { return apic_read_reg(APIC_REG_TMRCURRCNT); }

void write_ICR(ICR i)
{
    u32 *ptr = (u32 *)&i;
    apic_write_reg(APIC_ICR_HIGH, ptr[1]);
    apic_write_reg(APIC_ICR_LOW, ptr[0]);
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

void broadcast_sipi(u8 vector) { apic_write_reg(APIC_ICR_LOW, 0x000C4600 | vector); }

void broadcast_init_ipi() { apic_write_reg(APIC_ICR_LOW, 0x000C4500); }

void send_ipi_fixed(u8 vector, u32 dest)
{
    serial_logger.printf("[Kernel] Info: Sending IPI to %h with vector %h\n", dest, vector);
    apic_write_reg(APIC_ICR_HIGH, dest);
    apic_write_reg(APIC_ICR_LOW, (u32)vector | (0x01 << 14));
}

void send_ipi_fixed_others(u8 vector)
{
    serial_logger.printf("[Kernel] Info: Sending IPI to others with vector %h\n", vector);
    apic_write_reg(APIC_ICR_HIGH, 0);

    // Send to *vector* vector with Assert level and All Excluding Self
    // shorthand
    apic_write_reg(APIC_ICR_LOW, vector | (0x01 << 14) | (0b11 << 18));
}

void smart_eoi(u8 intno)
{
    u8 isr_index = intno >> 5;
    u8 offset    = intno & 0x1f;

    u32 isr_val = apic_read_reg(APIC_ISR_REG_START + isr_index * 0x10);

    if (isr_val & (0x01 << offset))
        apic_eoi();
}

void apic_pp(int priority) { apic_write_reg(APIC_REG_PPR, priority << 4); }

void lvt0_int_routine() {
    smart_eoi(LVT_INT0);
}

void lvt1_int_routine() {
    smart_eoi(LVT_INT1);
}

void apic_spurious_int_routine() {
    global_logger.printf("[Kernel] Info: Spurious LAPIC interrupt\n");
    serial_logger.printf("[Kernel] Info: Spurious LAPIC interrupt\n");
}

void apic_dummy_int_routine() {
    smart_eoi(APIC_DUMMY_ISR);
}

void interrupt_complete(u32 intno)
{
    assert(intno < 256);
    smart_eoi(intno);
    setCR8(0);
}

u32 interrupt_min() { return 48; }
u32 interrupt_max() { return 239; }

extern "C" void programmable_interrupt(u32 intno)
{
    auto c = get_cpu_struct();

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
        setCR8(14);
    }
}

void interrupt_enable(u32) {}
void interrupt_disable(u32) {}