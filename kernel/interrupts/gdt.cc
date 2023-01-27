#include "interrupts.hh"
#include "gdt.hh"
#include "utils.hh"
#include "sched/sched.hh"

TSS* System_Segment_Descriptor::tss()
{
    return (TSS*)((u64)base0 | (u64)base1 << 16 | (u64)base2 << 24 | (u64)base3 << 32);
}

void loadGDT(GDT *gdt)
{
    loadGDT(gdt, gdt->get_size());
}

void load_temp_gdt()
{
    loadGDT(&temp_gdt);
}

GDT temp_gdt;