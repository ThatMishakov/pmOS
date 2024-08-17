#include <interrupts.h>
#include <pmos/system.h>
#include <pmos/interrupts.h>
#include <errno.h>

int register_interrupt(uint32_t cpu_id, uint32_t vector, uint64_t task, pmos_port_t port)
{
    uint64_t self = get_task_id();
    task = task == 0 ? self : task;

    if (task != self) {
        // Retry 5 times in case someone is also trying to pause the task
        result_t result = 0;
        for (int i = 0; i < 5; ++i) {
            result = pause_task(task);
            if (result != 0) {
                return -EINVAL;
            }

            // Bind the task to the CPU
            result = set_affinity(task, cpu_id + 1, 0);
            if (result != -EBUSY)
                break;
        }
        if (result != 0) {
            return result;
        }
        // TODO: Save the old affinity to restore it in case of failure

        // Resume the task
        // It's not an error if this fails
        resume_task(task);
    }

    // Bind self to the particular CPU
    result_t r = set_affinity(self, cpu_id + 1, 0);
    if (r != 0) {
        return r;
    }

    // Set the interrupt
    syscall_r res = set_interrupt(port, vector, 0);
    if (res.result != 0) {
        return res.result;
    }

    // Bind to all CPUs if not self
    if (task != self) {
        r = set_affinity(self, 0, 0);
        if (r != 0) {
            return r;
        }
    }

    return 0;
}