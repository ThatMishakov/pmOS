#include "interrupts.hh"
#include "gdt.hh"
#include "utils.hh"

TSS* System_Segment_Descriptor::tss()
{
    return (TSS*)((u64)base0 | (u64)base1 << 16 | (u64)base2 << 24 | (u64)base3 << 32);
}

GDT kernel_gdt;

void init_gdt()
{
    loadGDT(&kernel_gdt, sizeof(GDT) - 1);
}