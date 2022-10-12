#include "gdt.hh"

GDT kernel_gdt;

void init_gdt()
{
    GDT_descriptor gdt_descriptor = {sizeof(GDT) - 1, (uint64_t)&kernel_gdt};
    loadGDT(&gdt_descriptor);
}