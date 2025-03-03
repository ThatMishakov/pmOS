#include "gdt.hh"

#include <sched/sched.hh>
#include <types.hh>

u32 segment_to_base(u64 segment)
{
    return ((segment >> 16) & 0xffffff) | ((segment >> 32) & 0xff000000);
}

u64 base_to_user_data_segment(u32 base)
{
    return 0x00cff2000000ffff | (((u64)base & 0xffffff) << 16) | (((u64)base & 0xff000000) << 32);
}

void gdt_set_cpulocal(CPU_Info *c)
{
    auto &gdt = c->cpu_gdt;
    gdt.kernel_gs =
        0x00cf92000000ffff | (((u64)c & 0xffffff) << 16) | (((u64)c & 0xff000000) << 32);

    // TODO: TSS
}

u64 tss_to_base(TSS *tss)
{
    u32 limit = sizeof(TSS) - 1; // Maybe not -1?
    return 0x0040890000000000 | (((u64)tss & 0xffffff) << 16) | (((u64)tss & 0xff000000) << 32) |
           (u64)limit;
}

TSS *getTSS(u16 selector)
{
    auto cpu_struct = get_cpu_struct();
    u64 *casual_ub  = (u64 *)&cpu_struct->cpu_gdt;
    return (TSS *)segment_to_base(casual_ub[selector/8]);
}

struct GDTR {
    u16 limit;
    u32 base;
} PACKED;

void loadGDT(GDT *gdt)
{
    GDTR gdtr = {
        .limit = sizeof(GDT) - 1,
        .base  = (u32)gdt,
    };

    asm volatile("lgdt %0\n"
                 "jmp $0x08, $1f\n"
                 "1:\n"
                 "movw $0x10, %%ax\n"
                 "movw %%ax, %%ss\n"
                 "movw $0x23, %%ax\n"
                 "movw %%ax, %%ds\n"
                 "movw %%ax, %%es\n"
                 "movw $0x38, %%ax\n" // Kernel %gs
                 "movw %%ax, %%gs\n"
                 "movw $0x33, %%ax\n"
                 "movw %%ax, %%fs\n" // User %fs
                 :
                 : "m"(gdtr)
                 : "memory", "eax");
}

void unbusyTSS(GDT *gdt)
{
    gdt->tss &= ~(1ULL << 41);
}

void loadTSS() { asm volatile("ltr %w0" : : "rm"(TSS_SEGMENT)); }