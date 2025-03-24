#include <csr.hh>
#include <interrupts.hh>
#include <kern_logger/kern_logger.hh>
#include <loongarch_asm.hh>
#include <paging/loongarch64_paging.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <types.hh>

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

void print_stack_trace_fp(u64 fp = get_fp(), Logger &logger = serial_logger)
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

void print_registers(LoongArch64Regs *regs, Logger &logger = serial_logger)
{
    logger.printf("Registers:\n");
    if (!regs) {
        logger.printf("NULL!!!\n");
    } else {
        logger.printf("PC: 0x%lx $ra: 0x%lx $tp: 0x%lx $sp: 0x%lx\n", regs->pc, regs->ra, regs->tp,
                      regs->sp);
        logger.printf("$a0: 0x%lx $a1: 0x%lx $a2: 0x%lx $a3: 0x%lx\n", regs->a0, regs->a1, regs->a2,
                      regs->a3);
        logger.printf("$a4: 0x%lx $a5: 0x%lx $a6: 0x%lx $a7: 0x%lx\n", regs->a4, regs->a5, regs->a6,
                      regs->a7);
        logger.printf("$t0: 0x%lx $t1: 0x%lx $t2: 0x%lx $t3: 0x%lx\n", regs->t0, regs->t1, regs->t2,
                      regs->t3);
        logger.printf("$t4: 0x%lx $t5: 0x%lx $t6: 0x%lx $t7: 0x%lx\n", regs->t4, regs->t5, regs->t6,
                      regs->t7);
        logger.printf("$t8: 0x%lx $r21: 0x%lx $fp: 0x%lx $s0: 0x%lx\n", regs->t8, regs->r21,
                      regs->fp, regs->s0);
        logger.printf("$s1: 0x%lx $s2: 0x%lx $s3: 0x%lx $s4: 0x%lx\n", regs->s1, regs->s2, regs->s3,
                      regs->s4);
        logger.printf("$s5: 0x%lx $s6: 0x%lx $s7: 0x%lx $s8: 0x%lx\n", regs->s5, regs->s6, regs->s7,
                      regs->s8);
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

unsigned exception_code(u32 estat = csrrd32<loongarch::csr::ESTAT>())
{
    return (estat >> 16) & 0x3f;
}

extern "C" void kernel_interrupt(LoongArch64Regs *regs)
{
    assert(regs);

    auto code = exception_code();

    do {
        if (code < 0x1 or code > 0x6)
            break;

        u64 virt_addr = csrrd64<loongarch::csr::BADV>();
        if (virt_addr < (1UL << 63)) {
            // Lower half
            auto c = get_cpu_struct();

            if (!c->jumpto_func)
                break;

            regs->pc = (ulong)c->jumpto_func;
            regs->a0 = c->jumpto_arg;
            regs->a1 = virt_addr;

            return;
        }

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

constexpr unsigned EXCEPTION_INT = 0x0;
constexpr unsigned EXCEPTION_PIL = 0x01;
constexpr unsigned EXCEPTION_PIS = 0x02;
constexpr unsigned EXCEPTION_PIF = 0x03;
constexpr unsigned EXCEPTION_PME = 0x04;
constexpr unsigned EXCEPTION_PNR = 0x05;
constexpr unsigned EXCEPTION_PNX = 0x06;
constexpr unsigned EXCEPTION_PPI = 0x07;
constexpr unsigned EXCEPTION_ADE = 0x08;
constexpr unsigned EXCEPTION_ALE = 0x09;
constexpr unsigned EXCEPTION_BCE = 0x0a;
constexpr unsigned EXCEPTION_SYS = 0x0b;

constexpr unsigned EXCEPTION_FPE  = 0x0f;
constexpr unsigned EXCEPTION_SXD  = 0x10;
constexpr unsigned EXCEPTION_ASXD = 0x11;

void page_fault(u32 error)
{
    u64 badv        = csrrd64<loongarch::csr::BADV>();
    void *virt_addr = (void *)(badv & ~0xfffUL);

    unsigned access_type = 0;
    switch (error) {
    case EXCEPTION_PIF:
    case EXCEPTION_PNX:
        access_type = Generic_Mem_Region::Executable;
        break;
    case EXCEPTION_PIS:
    case EXCEPTION_PME:
        access_type = access_type = Generic_Mem_Region::Writeable;
        break;
    case EXCEPTION_PIL:
    case EXCEPTION_PNR:
        access_type = Generic_Mem_Region::Readable;
        break;
    }

    auto task       = get_cpu_struct()->current_task;
    auto page_table = task->page_table;

    auto result = [&]() -> kresult_t {
        Auto_Lock_Scope lock(page_table->lock);

        auto &regions = page_table->paging_regions;
        auto it       = regions.get_smaller_or_equal(virt_addr);
        if (it != regions.end() and it->is_in_range(virt_addr)) {
            auto r = it->on_page_fault(access_type, virt_addr);
            if (!r.success())
                return r.result;

            if (not r.val) {
                task->atomic_block_by_page(virt_addr, &task->page_table->blocked_tasks);
            }

            return 0;
        } else
            return -EFAULT;
    }();

    if (result) {
        static Spinlock print_lock;
        Auto_Lock_Scope lock(print_lock);

        serial_logger.printf("Warning: Pagefault %h pid %i (%s) rip %h error "
                             "%h -> %i killing process...\n",
                             virt_addr, task->task_id, task->name.c_str(), task->regs.pc, badv,
                             result);
        global_logger.printf("Warning: Pagefault %h pid %i (%s) pc %h error %h "
                             "-> %i killing process...\n",
                             virt_addr, task->task_id, task->name.c_str(), task->regs.pc, badv,
                             result);
        print_registers(&task->regs, global_logger);
        print_registers(&task->regs, serial_logger);
        task->atomic_kill();
    }
}

extern "C" void syscall_handler();

kresult_t handle_fp_disabled_exception(unsigned code);
void handle_hardware_interrupt(u32 estat);

extern "C" void handle_interrupt()
{
    assert(get_cpu_struct()->nested_level == 1);
    u32 estat = csrrd32<loongarch::csr::ESTAT>();
    auto code = exception_code(estat);
    estat &= csrrd32<loongarch::csr::ECFG>();
    auto c    = get_cpu_struct();

    switch (code) {
    case EXCEPTION_INT: {
        assert(estat);

        if (estat & TIMER_INT_MASK) {
            csrwr<loongarch::csr::TICLR>(0x01);
            sched_periodic();
        }

        if (estat & HARDWARE_INT_MASK) {
            handle_hardware_interrupt(estat);
        }
    } break;
    case EXCEPTION_PIL:
    case EXCEPTION_PIS:
    case EXCEPTION_PIF:
    case EXCEPTION_PME:
    case EXCEPTION_PNR:
    case EXCEPTION_PNX:
        page_fault(code);
        break;

    case EXCEPTION_SYS:
        c->current_task->regs.program_counter() += 4;
        syscall_handler();
        break;

    case EXCEPTION_FPE:
    case EXCEPTION_SXD:
    case EXCEPTION_ASXD: {
        auto result = handle_fp_disabled_exception(code);
        if (result) {
            auto task = get_current_task();
            serial_logger.printf("Task %li (%s) is using unsupported FP/vector instructions (code "
                                 "%i). Killing it...\n",
                                 task->task_id, task->name.c_str(), code);
            print_registers(&task->regs, serial_logger);
            task->atomic_kill();
        }

    } break;

    default: {
        auto task = get_current_task();
        serial_logger.printf("Userspace (or idle) interrupt\n");
        serial_logger.printf("Exception 0x%x\n", code);
        print_registers(&task->regs);
        serial_logger.printf("Task %li (%s)\n", task->task_id, task->name.c_str());
        panic("Unimplemented exception!\n");
    }
    }

    while (c->current_task->regs.syscall_restart != 0) {
        c->current_task->regs.a0 = c->current_task->syscall_num;
        c->current_task->regs.syscall_restart = 0;
        syscall_handler();
    }
    assert(c->nested_level == 1);
}

void printc(int) {}

extern "C" void allow_access_user() {}
extern "C" void disallow_access_user() {}
