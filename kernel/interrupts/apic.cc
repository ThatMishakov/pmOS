#include "apic.hh"
#include <asm.hh>
#include <memory/free_page_alloc.hh>
#include <memory/paging.hh>
#include "pic.hh"
#include "pit.hh"

void* apic_mapped_addr = nullptr;

void map_apic()
{
    apic_mapped_addr = (void*)global_free_page.get_free_page().val;
    map(apic_base, (u64)apic_mapped_addr, {1,0,0,0,PAGE_SPECIAL});
}

void enable_apic()
{
    cpu_set_apic_base(cpu_get_apic_base());
    apic_write_reg(APIC_REG_SPURIOUS_INT, APIC_SPURIOUS_INT | 0x100);
}

void init_apic()
{
    init_PIC();
    map_apic();

    apic_write_reg(APIC_REG_DFR, 0xffffffff);
    apic_write_reg(APIC_REG_LDR, 0x01000000);
    apic_write_reg(APIC_REG_LVT_TMR, APIC_LVT_MASK);
    apic_write_reg(APIC_REG_LVT_INT0, LVT_INT0);
    apic_write_reg(APIC_REG_LVT_INT1, LVT_INT1);

    enable_apic();
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
    t_print("Info: APIC timer ticks per 1ms: %h\n", ticks_per_1_ms);
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
    u32* reg = (u32*)((u64)apic_mapped_addr + index);
    *reg = val;
}

u32 apic_read_reg(u16 index)
{
    u32* reg = (u32*)((u64)apic_mapped_addr + index);
    return *reg;
}

u32 ticks_per_1_ms = 0;