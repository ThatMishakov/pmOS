#include "apic.hh"
#include <asm.hh>
#include <memory/free_page_alloc.hh>
#include <memory/paging.hh>
#include "pic.hh"

void* apic_mapped_addr = nullptr;

void map_apic()
{
    apic_mapped_addr = (void*)global_free_page.get_free_page().val;
    map(apic_base, (u64)apic_mapped_addr, {1,0,0,0,PAGE_SPECIAL});
}

void enable_apic()
{
    cpu_set_apic_base(apic_base);
    apic_write_reg(0xF0, APIC_SPURIOUS_INT | 0x100);
}

void init_apic()
{
    init_PIC();
    map_apic();
    enable_apic();
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