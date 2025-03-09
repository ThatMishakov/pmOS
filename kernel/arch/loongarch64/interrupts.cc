#include <types.hh>
#include <kern_logger/kern_logger.hh>
#include <sched/sched.hh>
#include <loongarch_asm.hh>

constexpr u32 IOCSR_IPI_SEND = 0x1040;

struct fp_s {
    u64 fp;
    u64 ra;
};

extern "C" CPU_Info *get_cpu_struct()
{
    CPU_Info * c;
    asm("move %0, $tp" : "=r"(c));
    return c;
}

u64 get_fp()
{
    u64 fp;
    asm("move %0, $fp" : "=r"(fp));
    return fp;
}

void print_stack_trace_fp(u64 fp = get_fp())
{
    fp_s *current = (fp_s *)((char *)fp - 16);
    serial_logger.printf("Stack trace:\n");
    for (int i = 0; i < 20; i++) {
        serial_logger.printf("  0x%x fp 0x%x\n", current->ra, current->fp);
        if (current->fp == 0 or ((i64)current->fp > 0)) {
            break;
        }
        current = (fp_s *)current->fp - 1;
    }
}

extern "C" void print_stack_trace() { print_stack_trace_fp(); }

void ipi_send(u32 cpu, u32 vector)
{
    uint32_t value = (cpu << 16) | vector;
    iocsr_write32(value, IOCSR_IPI_SEND);
}

void CPU_Info::ipi_reschedule()
{
    __atomic_or_fetch(&ipi_mask, IPI_RESCHEDULE, __ATOMIC_ACQUIRE);
    ipi_send(cpu_physical_id, 0x00);
}