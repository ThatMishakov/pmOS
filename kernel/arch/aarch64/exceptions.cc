#include <types.hh>
#include <utils.hh>
#include <arm_asm.hh>
#include <kern_logger/kern_logger.hh>
#include <registers.hh>
#include <signal.h>
#include <memory/paging.hh>
#include <sched/sched.hh>
#include <processes/tasks.hh>

using namespace kernel;

DEFINE_SYSREG(esr_el1)
DEFINE_SYSREG(far_el1)

extern "C" void unexpected_exception()
{
    panic("Unexpected exception!\n");
}

namespace {

void print_registers(Task_Regs *regs, log::Logger &logger)
{
    logger.printf("Registers:\n");
    for (int i = 0; i < 31; ++i) {
        logger.printf("x%02d: %016lx\n", i, regs->x[i]);
    }
    logger.printf("pc: %016lx\n", regs->pc);
    logger.printf("sp: %016lx\n", regs->sp);
    logger.printf("spsr_el1: %016lx\n", regs->spsr_el1);
    logger.printf("tpidr_el0: %016lx\n", regs->tpidr_el0);
    logger.printf("tpidrro_el0: %016lx\n", regs->tpidrro_el0);
}


constexpr u32 USER_DATA_ABORT_EC = 0x24;
constexpr u32 USER_INSTRUCTION_ABORT_EC = 0x20;

u32 ec_from_esr(u64 esr)
{
    return (esr >> 26) & 0x3f;
}

void handle_user_abort(u64 esr)
{
    ulong fault_addr = mrs<far_el1>();

    unsigned access_type = 0;

    u32 ec = ec_from_esr(esr);
    if (ec == USER_INSTRUCTION_ABORT_EC) {
        access_type = paging::Generic_Mem_Region::Executable;
    } else {
        assert(ec == USER_DATA_ABORT_EC);
        bool write = esr & (1 << 6);
        if (write)
            access_type = paging::Generic_Mem_Region::Writeable;
        else
            access_type = paging::Generic_Mem_Region::Readable;
    }

    ReturnStr<bool> result = true;
    u32 dfsc = esr & 0x3f;
    switch (dfsc) {
    case 0x04 ... 0x07:
        // Translation fault
        break;
    case 0x08 ... 0x0b:
        // Access flag fault
        break;
    case 0x0c ... 0x0f:
        // Permission fault
        break;

    // TODO
    default:
        panic("Unhandled user abort, ESR_EL1 0x%lx dfsc 0x%x", esr, dfsc);
    }

    auto task       = sched::get_cpu_struct()->current_task;
    auto page_table = task->page_table;
    ulong PAGE_MASK = PAGE_SIZE - 1;
    void *virt_addr = (void *)(fault_addr & ~PAGE_MASK);

    if (result)
        result = page_table->atomic_page_fault(virt_addr, access_type);
    
    if (!result) {
        static Spinlock print_lock;
        Auto_Lock_Scope lock(print_lock);

        log::serial_logger.printf("Warning: Pagefault %h pid %i (%s) rip %h error "
                             "%h -> %i killing process...\n",
                             virt_addr, task->task_id, task->name.c_str(), task->regs.pc, fault_addr,
                             result.result);
        log::global_logger.printf("Warning: Pagefault %h pid %i (%s) pc %h error %h "
                             "-> %i killing process...\n",
                             virt_addr, task->task_id, task->name.c_str(), task->regs.pc, fault_addr,
                             result.result);
        print_registers(&task->regs, log::global_logger);
        print_registers(&task->regs, log::serial_logger);
        task->atomic_kill(SIGSEGV + 128);
    }
}

}

extern "C" void handle_user_sync()
{
    u64 esr = mrs<esr_el1>();

    log::serial_logger.printf("Synchronous user int esr_el1 %lx...\n", esr);

    u32 ec = ec_from_esr(esr);
    switch (ec) {
    case USER_INSTRUCTION_ABORT_EC:
        handle_user_abort(esr);
        break;

    default:
        panic("Unhandled synchronous user exception class 0x%x, ESR_EL1 0x%lx", ec, esr);
        break;
    }
}

extern "C" void handle_user_irq()
{
    panic("Unhandled handle_user_irq");
}

extern "C" void handle_user_fiq()
{
    panic("Unhandled handle_user_fiq");
}

extern "C" void handle_user_serror()
{
    panic("Unhandled handle_user_serror");
}