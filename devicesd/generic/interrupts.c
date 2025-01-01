#include <interrupts.h>
#include <pmos/system.h>
#include <pmos/interrupts.h>
#include <errno.h>
#include <pmos/ipc.h>
#include <string.h>

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

void configure_interrupts_for(Message_Descriptor *desc, IPC_Reg_Int *m)
{
    uint32_t gsi = 0;
    bool active_low = false;
    bool level_trig = false;
    int result = 0;

    uint32_t vector = 0;

    if (m->flags & IPC_Reg_Int_FLAG_EXT_INTS) {
        int_redirect_descriptor desc = isa_gsi_mapping(m->intno);
        gsi = desc.destination;
        active_low = desc.active_low;
        level_trig = desc.level_trig;
    } else {
        gsi = m->intno;
        // active_low = m->active_low;
        // level_trig = m->level_trig;
    }

    if (m->dest_task == 0) {
        result = -EINVAL;
    } else {
        result = set_up_gsi(gsi, active_low, level_trig, m->dest_task, m->dest_chan, &vector);
    }
    
    IPC_Reg_Int_Reply reply;
    reply.type = IPC_Reg_Int_Reply_NUM;
    reply.status = result;
    reply.intno = vector;
    result = send_message_port(m->reply_chan, sizeof(reply), (char*)&reply);
    if (result < 0)
        fprintf(stderr, "Warning could not reply to task %#lx port %#lx in configure_interrupts_for: %i (%s)\n", desc->sender, m->reply_chan, result, strerror(-result));
}