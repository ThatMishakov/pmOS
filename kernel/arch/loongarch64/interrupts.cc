#include <csr.hh>
#include <kern_logger/kern_logger.hh>
#include <loongarch_asm.hh>
#include <sched/sched.hh>
#include <types.hh>
#include <paging/loongarch64_paging.hh>

using namespace kernel;

constexpr u32 IOCSR_IPI_SEND = 0x1040;

struct fp_s {
    u64 fp;
    u64 ra;
};

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
        serial_logger.printf("  0x%lx fp 0x%lx\n", current->ra, current->fp);
        if (current->fp == 0 or ((i64)current->fp > 0)) {
            break;
        }
        current = (fp_s *)current->fp - 1;
    }
}

extern "C" void print_stack_trace() { print_stack_trace_fp(); }

void print_registers(LoongArch64Regs *regs)
{
    serial_logger.printf("Registers:\n");
    if (!regs) {
        serial_logger.printf("NULL!!!\n");
    } else {
        serial_logger.printf("PC: 0x%lx $ra: 0x%lx $tp: 0x%lx $sp: 0x%lx\n", regs->pc, regs->ra,
                             regs->tp, regs->sp);
        serial_logger.printf("$a0: 0x%lx $a1: 0x%lx $a2: 0x%lx $a3: 0x%lx\n", regs->a0, regs->a1,
                             regs->a2, regs->a3);
        serial_logger.printf("$a4: 0x%lx $a5: 0x%lx $a6: 0x%lx $a7: 0x%lx\n",
                             regs->a4, regs->a5, regs->a6, regs->a7);
        serial_logger.printf("$t0: 0x%lx $t1: 0x%lx $t2: 0x%lx $t3: 0x%lx\n",
                             regs->t0, regs->t1, regs->t2, regs->t3);
        serial_logger.printf("$t4: 0x%lx $t5: 0x%lx $t6: 0x%lx $t7: 0x%lx\n",
                             regs->t4, regs->t5, regs->t6, regs->t7);
        serial_logger.printf("$t8: 0x%lx $r21: 0x%lx $fp: 0x%lx $s0: 0x%lx\n",
                             regs->t8, regs->r21, regs->fp, regs->s0);
        serial_logger.printf("$s1: 0x%lx $s2: 0x%lx $s3: 0x%lx $s4: 0x%lx\n",
                             regs->s1, regs->s2, regs->s3, regs->s4);
        serial_logger.printf("$s5: 0x%lx $s6: 0x%lx $s7: 0x%lx $s8: 0x%lx\n",
                             regs->s5, regs->s6, regs->s7, regs->s8);
    }
}

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

void CPU_Info::ipi_tlb_shootdown()
{
    __atomic_or_fetch(&ipi_mask, IPI_TLB_SHOOTDOWN, __ATOMIC_ACQUIRE);
    ipi_send(cpu_physical_id, 0x00);
}

void interrupt_disable(u32 interrupt_id) { panic("interrupt_disable: not implemented\n"); }

unsigned exception_code()
{
    auto reg = csrrd32<loongarch::csr::ESTAT>();
    return (reg >> 16) & 0x3f;
}

extern "C" void kernel_interrupt(LoongArch64Regs *regs)
{
    assert(regs);

    auto code = exception_code();

    do {
        if (code < 0x1 or code > 0x6)
            break;

        u64 virt_addr = csrrd64<loongarch::csr::BADV>();
        if (virt_addr < (1UL << 63)) // Lower half
            break;

        auto page_table = csrrd64<loongarch::csr::PGDH>();

        auto mapping = get_page_mapping(page_table, (void *)virt_addr);
        if (!mapping.is_allocated)
            break;

        if ((code == 0x1 or code == 0x5) and !mapping.readable)
            break;

        if ((code == 0x2 or code == 0x4) and !mapping.writeable)
            break;

        if ((code == 0x3 or code == 0x6) and !mapping.executable)
            break;

        invalidate_kernel_page((void *)virt_addr, 0);
        return;
    } while (0);

    u64 virt_addr = csrrd64<loongarch::csr::BADV>();
    serial_logger.printf("BADV: 0x%lx\n", virt_addr);

    print_registers(regs);
    print_stack_trace_fp(regs->fp);
    panic("Kernel interrupt %i!\n", code);
}

extern "C" void handle_interrupt() {
    panic("Interrupts not implemented!\n");
}

void printc(int)
{
    
}