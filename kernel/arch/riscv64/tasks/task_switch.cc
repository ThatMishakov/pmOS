#include <processes/tasks.hh>
#include <cpus/floating_point.hh>
#include <sched/sched.hh>

void TaskDescriptor::before_task_switch() {
    u32 fp_state = get_fp_state();
    if (fp_state == FloatingPointState::Disabled) {
        return;
    }

    auto c = get_cpu_struct();

    switch (fp_state) {
    case FloatingPointState::Disabled:
        break;
    case FloatingPointState::Initial:
        this->fp_state = FloatingPointState::Initial;
        this->last_fp_hart_id = c->hart_id;
        c->last_fp_task = pid;
        c->last_fp_state = FloatingPointState::Initial;
        break;
    case FloatingPointState::Clean:
        this->fp_state = FloatingPointState::Clean;
        this->last_fp_hart_id = c->hart_id;
        c->last_fp_task = pid;
        c->last_fp_state = FloatingPointState::Clean;
        break;
    case FloatingPointState::Dirty:
        if (!this->fp_registers)
            this->fp_registers = klib::unique_ptr<u64[]>(new u64[fp_register_size(max_supported_fp_level)*2]);
        
        asm("csrr %0, fcsr" : "=r"(this->fcsr));
        save_fp_registers(fp_registers.get());
        this->fp_state = FloatingPointState::Clean;
        this->last_fp_hart_id = c->hart_id;
        c->last_fp_task = pid;
        c->last_fp_state = FloatingPointState::Clean;
        break;
    }

    set_fp_state(FloatingPointState::Disabled);
}

void TaskDescriptor::after_task_switch() {
    auto c = get_cpu_struct();
    if ((fp_state == FloatingPointState::Disabled or fp_state == FloatingPointState::Initial) and c->last_fp_state == FloatingPointState::Initial) {
        set_fp_state(FloatingPointState::Initial);
        return;
    }

    if (c->last_fp_task == pid and last_fp_hart_id == c->hart_id) {
        set_fp_state(FloatingPointState::Clean);
        return;
    }
}