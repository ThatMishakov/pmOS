#include <configuration.h>
#include <ioapic/ioapic.h>
#include <devicesd/devicesd_msgs.h>
#include <interrupts/interrupts.h>
#include <stdbool.h>
#include <pmos/system.h>

uint8_t configure_interrupts_for(DEVICESD_MESSAGE_REG_INT* m)
{
    if (m->flags & MSG_REG_INT_FLAG_EXTERNAL_INTS) {
        uint8_t cpu_int_vector = get_free_interrupt();

        result_t kern_result = set_port_kernel(KERNEL_MSG_INT_START+cpu_int_vector, m->dest_task, m->dest_chan);
        if (kern_result != SUCCESS) return 0;

        bool result = program_ioapic(cpu_int_vector, m->intno);
        return result ? cpu_int_vector : 0;
    }

    // Not supported yet
    return 0;
}