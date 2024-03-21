#include "floating_point.hh"
#include <processes/tasks.hh>
#include <assert.h>
#include <sched/sched.hh>

extern "C" void fp_write_regs_initial();
extern "C" void fp_load_regs_single(const u64* fp_regs);
extern "C" void fp_load_regs_double(const u64* fp_regs);
extern "C" void fp_store_regs_single(u64* fp_regs);
extern "C" void fp_store_regs_double(u64* fp_regs);

void set_fp_state_initial()
{
    set_fp_state(FloatingPointState::Initial);

    static const u64 zero = 0;
    asm("csrw fcsr, %0" : : "r"(zero));

    fp_write_regs_initial();

    set_fp_state(FloatingPointState::Initial);
}

void restore_fp_state_single(TaskDescriptor* task)
{
    assert(task != nullptr and task->fp_registers);

    set_fp_state(FloatingPointState::Dirty);

    asm("csrw fcsr, %0" : : "r"(task->fcsr));
    const u64 *fp_regs = task->fp_registers.get();
    fp_load_regs_single(fp_regs);

    set_fp_state(FloatingPointState::Clean);
}

void restore_fp_state_double(TaskDescriptor* task)
{
    assert(task != nullptr and task->fp_registers);

    set_fp_state(FloatingPointState::Dirty);

    asm("csrw fcsr, %0" : : "r"(task->fcsr));
    const u64 *fp_regs = task->fp_registers.get();
    fp_load_regs_double(fp_regs);

    set_fp_state(FloatingPointState::Clean);
}

void restore_fp_state(TaskDescriptor* task)
{
    assert(task != nullptr and not (task->fp_state == FloatingPointState::Clean and !task->fp_registers));

    if (task->fp_state == FloatingPointState::Disabled or task->fp_state == FloatingPointState::Initial) {
        set_fp_state_initial();
        return;
    }

    switch (max_supported_fp_level) {
    case FloatingPointSize::SinglePrecision:
        restore_fp_state_single(task);
        break;
    case FloatingPointSize::DoublePrecision:
        restore_fp_state_double(task);
        break;
    default:
        assert(false);
    }
}

void save_fp_registers(u64* fp_regs)
{
    assert(fp_regs);

    switch (max_supported_fp_level) {
    case FloatingPointSize::SinglePrecision:
        fp_store_regs_single(fp_regs);
        break;
    case FloatingPointSize::DoublePrecision:
        fp_store_regs_double(fp_regs);
        break;
    default:
        assert(false);
    }
}