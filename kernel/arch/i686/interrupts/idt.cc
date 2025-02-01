#include "idt.hh"

#include "gdt.hh"

constexpr u64 task_gate(u16 selector) { return 0x0000'8500'0000'0000 | (u64(selector) << 16); }

constexpr u64 interrupt_gate(u32 function, u16 cpl)
{
    return 0x0000'8e00'0000'0000 | ((u64)function & 0xffff) | (u64(cpl) << 45) |
           (u64(function & 0xffff0000) << 32) | (R0_CODE_SEGMENT << 16);
}

constexpr unsigned DIVIDE_INT                      = 0;
constexpr unsigned DEBUG_INT                       = 1;
constexpr unsigned NMI_INT                         = 2;
constexpr unsigned BREAKPOINT_INT                  = 3;
constexpr unsigned OVERFLOW_INT                    = 4;
constexpr unsigned BOUNDS_INT                      = 5;
constexpr unsigned INVALID_OPCODE_INT              = 6;
constexpr unsigned DEVICE_NOT_AVAILABLE_INT        = 7;
constexpr unsigned DOUBLE_FAULT_INT                = 8;
constexpr unsigned COPROCESSOR_SEGMENT_OVERRUN_INT = 9;
constexpr unsigned INVALID_TSS_INT                 = 10;
constexpr unsigned SEGMENT_NOT_PRESENT_INT         = 11;
constexpr unsigned STACK_SEGMENT_FAULT_INT         = 12;
constexpr unsigned GENERAL_PROTECTION_INT          = 13;
constexpr unsigned PAGE_FAULT_INT                  = 14;
constexpr unsigned RESERVED_INT                    = 15;
constexpr unsigned X87_FPU_FP_ERROR_INT            = 16;
constexpr unsigned ALIGNMENT_CHECK_INT             = 17;
constexpr unsigned MACHINE_CHECK_INT               = 18;
constexpr unsigned SIMD_FP_EXCEPTION_INT           = 19;
constexpr unsigned VIRTUALIZATION_INT              = 20;

constexpr unsigned SYSCALL_INT    = 0xf8;
constexpr unsigned APIC_TIMER_INT = 0xfc;
constexpr unsigned APIC_SPURIOUS_INT = 0xff;

extern "C" void general_protection_fault_isr();
extern "C" void page_fault_isr();
extern "C" void sse_exception_isr();
extern "C" void syscall_isr();
extern "C" void apic_timer_isr();
extern "C" void apic_spurious_isr();

static IDT init_idt()
{
    IDT idt = {};
    auto &u = idt.entries;

    // u[DIVIDE_INT]
    // u[DEBUG_INT] = task_gate(DEBUG_TSS_SEGMENT);
    // u[NMI_INT] = task_gate(NMI_TSS_SEGMENT);
    // u[BREAKPOINT_INT]
    // u[OVERFLOW_INT]
    // u[BOUNDS_INT]
    // u[INVALID_OPCODE_INT]
    u[DEVICE_NOT_AVAILABLE_INT] = interrupt_gate((u32)sse_exception_isr, 0);
    // u[DOUBLE_FAULT_INT] = task_gate(DOUBLE_FAULT_TSS_SEGMENT);
    // u[COPROCESSOR_SEGMENT_OVERRUN_INT]
    // u[INVALID_TSS_INT]
    // u[SEGMENT_NOT_PRESENT_INT]
    // u[STACK_SEGMENT_FAULT_INT] = task_gate(STACK_FAULT_TSS_SEGMENT);
    u[GENERAL_PROTECTION_INT]   = interrupt_gate((u32)general_protection_fault_isr, 0);
    u[PAGE_FAULT_INT]           = interrupt_gate((u32)page_fault_isr, 0);
    // u[RESERVED_INT]
    // u[X87_FPU_FP_ERROR_INT]
    // u[ALIGNMENT_CHECK_INT]
    // u[MACHINE_CHECK_INT] = task_gate(MACHINE_CHECK_TSS_SEGMENT);
    // u[SIMD_FP_EXCEPTION_INT]
    // u[VIRTUALIZATION_INT]

    u[SYSCALL_INT] = interrupt_gate((u32)syscall_isr, 3);
    u[APIC_TIMER_INT] = interrupt_gate((u32)apic_timer_isr, 0);
    u[APIC_SPURIOUS_INT] = interrupt_gate((u32)apic_spurious_isr, 0);

    return idt;
}

IDT k_idt = init_idt();