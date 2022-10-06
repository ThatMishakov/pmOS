#include <gdt.h>
#include "../kernel/common/gdt.h"
#include <mem.h>

GDT_entry *gdt = 0;
int gdt_size = 0;

GDT_entry *GDT_add_entry(GDT_entry entry)
{
    gdt[gdt_size++] = entry;
    return &gdt[gdt_size - 1];
}

void prepare_GDT()
{
    if (!gdt) gdt = (GDT*)alloc_page();
    GDT_add_entry((GDT_entry){0});
    GDT_add_entry((GDT_entry){0xffff, 0, 0, 0x9a, 0x80, 0});
    GDT_add_entry((GDT_entry){0xffff, 0, 0, 0x92, 0x80, 0});

    /*
    gdt->_64bit_code = (GDT_entry){0, 0, 0, 0x9a, 0xa2, 0}; // 64bit_code
    gdt->_64bit_data = (GDT_entry){0, 0, 0, 0x92, 0xa0, 0}; // 64bit_data
    */
    
}

void init_GDT()
{
    prepare_GDT();
    GDT_descriptor_m32 gdt_descriptor;
    gdt_descriptor.size = gdt_size;
    gdt_descriptor.offset = (uint32_t)gdt;
    loadGDT(&gdt_descriptor);
}