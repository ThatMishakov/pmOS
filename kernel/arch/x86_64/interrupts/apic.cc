#include "apic.hh"
#include <x86_asm.hh>
#include <x86_utils.hh>
#include <memory/paging.hh>
#include "pic.hh"
#include "pit.hh"
#include <kernel/errors.h>
#include <utils.hh>
#include <kern_logger/kern_logger.hh>
#include <exceptions.hh>
#include <memory/virtmem.hh>


void* apic_mapped_addr = nullptr;

void map_apic()
{
    apic_mapped_addr = virtmem_alloc(1);
    map_kernel_page(apic_base, apic_mapped_addr, {1,1,0,0,0,PAGE_SPECIAL});
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

void discover_apic_freq()
{
    // Enable APIC Timer and map to dummy ISR
    apic_write_reg(APIC_REG_LVT_TMR, APIC_DUMMY_ISR); 

    // Set up divide value to 16
    apic_write_reg(APIC_REG_TMRDIV, 0x03);   

    // Play with keyboard controller since channel 2 timer is 
    // connected to it... (I don't undertstand this)
    u8 p = inb(0x61);
    p = (p&0xfd) | 1;
    outb(0x61, p);

    outb(PIT_MODE_REG, 0b10'11'001'0);
    set_pit_count(0x2e9b, 2);


    // Start timer 2
    p = inb(0x61);
    p = (p&0xfe); 
    outb(0x61, p); // Gate LOW
    outb(0x61, p | 1); // Gate HIGH

    // Reset APIC counter
    apic_write_reg(APIC_REG_TMRINITCNT, (u32)-1);

    // Wait for PIT timer 2 to reach 0
    while (not (inb(0x61)&0x20));

    // Stop APIC timer
    apic_write_reg(APIC_REG_LVT_TMR, APIC_LVT_MASK);

    // Get how many ticks have passed
    u32 ticks = apic_read_reg(APIC_REG_TMRCURRCNT);

    ticks_per_1_ms = (0-ticks)*16/10;
    global_logger.printf("[Kernel] Info: APIC timer ticks per 1ms: %h\n", ticks_per_1_ms);
}

void apic_one_shot(u32 ms)
{
    u32 ticks = ticks_per_1_ms*ms/16;
    apic_write_reg(APIC_REG_TMRDIV, 0x3); // Divide by 16
    apic_write_reg(APIC_REG_LVT_TMR, APIC_TMR_INT); // Init in one-shot mode
    apic_write_reg(APIC_REG_TMRINITCNT, ticks);
}

void apic_one_shot_ticks(u32 ticks)
{
    apic_write_reg(APIC_REG_TMRDIV, 0b1011); // Divide by 1
    apic_write_reg(APIC_REG_LVT_TMR, APIC_TMR_INT); // Init in one-shot mode
    apic_write_reg(APIC_REG_TMRINITCNT, ticks);
}

u64 cpu_get_apic_base()
{
    return read_msr(IA32_APIC_BASE_MSR);
}

void cpu_set_apic_base(u64 base)
{
    return write_msr(IA32_APIC_BASE_MSR, base);
}

void apic_write_reg(u16 index, u32 val)
{
    volatile u32* reg = (volatile u32*)((u64)apic_mapped_addr + index);
    *reg = val;
}

u32 apic_read_reg(u16 index)
{
    volatile u32* reg = (volatile u32*)((u64)apic_mapped_addr + index);
    return *reg;
}

u32 ticks_per_1_ms = 0;

void apic_eoi()
{
    apic_write_reg(APIC_REG_EOI, 0);
}

u32 apic_get_remaining_ticks()
{
    return apic_read_reg(APIC_REG_TMRCURRCNT);
}

void write_ICR(ICR i)
{
    u32* ptr = (u32*)&i;
    apic_write_reg(APIC_ICR_HIGH, ptr[1]);
    apic_write_reg(APIC_ICR_LOW, ptr[0]);
}

u64 lapic_configure(u64 opt, u64 arg)
{
    u64 result = {0};

    switch (opt) {
    case 0:
        result = get_lapic_id();
        break;
    case 1:
        broadcast_init_ipi();
        break;
    case 2:
        broadcast_sipi(arg);
        break;
    case 3:
        send_ipi_fixed(arg >> 8, arg);
        break;
    default:
        throw(Kern_Exception(ERROR_NOT_SUPPORTED, "lapic_configure with unsupported parameter\n"));
    };
    return result;
}

u32 get_lapic_id()
{
    return apic_read_reg(APIC_REG_LAPIC_ID);
}

void broadcast_sipi(u8 vector)
{
    apic_write_reg(APIC_ICR_LOW,0x000C4600 | vector);
}

void broadcast_init_ipi()
{
    apic_write_reg(APIC_ICR_LOW,0x000C4500);
}

void send_ipi_fixed(u8 vector, u32 dest)
{
    apic_write_reg(APIC_ICR_HIGH, dest);
    apic_write_reg(APIC_ICR_LOW, vector | (0x01 << 14));
}

void send_ipi_fixed_others(u8 vector)
{
    apic_write_reg(APIC_ICR_HIGH, 0);

    // Send to *vector* vector with Assert level and All Excluding Self shorthand
    apic_write_reg(APIC_ICR_LOW, vector | (0x01 << 14) | (0b11 << 18));
}

void smart_eoi(u8 intno)
{
    u8 isr_index = intno >> 5;
    u8 offset = intno & 0x1f;

    u32 isr_val = apic_read_reg(APIC_ISR_REG_START + isr_index*0x10);

    if (isr_val & (0x01 << offset)) apic_eoi();
}

void lvt0_int_routine()
{

}

void lvt1_int_routine()
{

}
