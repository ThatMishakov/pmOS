#pragma once
#include <types.hh>

namespace kernel
{
namespace sched
{
    struct CPU_Info;
}

namespace x86_64
{

#define R0_CODE_SEGMENT            0x08
#define R0_DATA_SEGMENT            0x10
#define R3_CODE_SEGMENT            0x1b
#define R3_DATA_SEGMENT            0x23
#define USER_GS_SEGMENT            0x2b
#define USER_FS_SEGMENT            0x33
#define KERNEL_GS_SEGMENT          0x38
#define KERNEL_THREAD_CODE_SEGMENT 0x70

    struct GDT {
        u64 null               = 0;
        u64 kernel_code        = 0x00cf9a000000ffff;
        u64 kernel_data        = 0x00cf92000000ffff;
        u64 ring3_code         = 0x00cffa000000ffff;
        u64 ring3_data         = 0x00cff2000000ffff;
        u64 user_gs            = 0x00cff2000000ffff;
        u64 user_fs            = 0x00cff2000000ffff;
        u64 kernel_gs          = 0x00cff2000000ffff;
        u64 tss                = 0;
        u64 nmi_tss            = 0;
        u64 double_fault_tss   = 0;
        u64 machine_check_tss  = 0;
        u64 debug_tss          = 0;
        u64 stack_fault_tss    = 0;
        u64 kernel_thread_code = 0x00cf9a000000ffff;
    };

    struct TSS {
        u32 link {};
        u32 esp0 {};
        u32 ss0 {0x10};
        u32 esp1 {};
        u32 ss1 {};
        u32 esp2 {};
        u32 ss2 {};
        u32 cr3 {};
        u32 eip {};
        u32 eflags {};
        u32 eax {};
        u32 ecx {};
        u32 edx {};
        u32 ebx {};
        u32 esp {};
        u32 ebp {};
        u32 esi {};
        u32 edi {};
        u32 es {0x10};
        u32 cs {0x08};
        u32 ss {0x10};
        u32 ds {0x10};
        u32 fs {0x10};
        u32 gs {0x38};
        u32 ldt {};
        u16 trap {};
        u16 iomap_base {sizeof(TSS)};
        u32 ssp {};
    };
    static_assert(sizeof(TSS) == 108);

    constexpr unsigned TSS_SEGMENT               = 0x40;
    constexpr unsigned NMI_TSS_SEGMENT           = 0x48;
    constexpr unsigned DOUBLE_FAULT_TSS_SEGMENT  = 0x50;
    constexpr unsigned MACHINE_CHECK_TSS_SEGMENT = 0x58;
    constexpr unsigned DEBUG_TSS_SEGMENT         = 0x60;
    constexpr unsigned STACK_FAULT_TSS_SEGMENT   = 0x68;

    u32 segment_to_base(u64 segment);
    u64 base_to_user_data_segment(u32 base);
    u64 base_to_kernel_data_segment(u32 base);
    u64 tss_to_base(TSS *tss);
    TSS *getTSS(u16 selector);

    void loadGDT(GDT *gdt);
    void loadTSS();
    void gdt_set_cpulocal(sched::CPU_Info *c);
    void unbusyTSS(GDT *gdt);

} // namespace x86_64
} // namespace kernel