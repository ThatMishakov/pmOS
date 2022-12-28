#include <configuration.h>
#include <ioapic/ioapic.h>
#include <devicesd/devicesd_msgs.h>
#include <interrupts/interrupts.h>
#include <stdbool.h>
#include <pmos/system.h>

uint8_t get_ioapic_int(uint32_t intno, uint64_t dest_pid, uint32_t chan)
{
    uint8_t cpu_int_vector = get_free_interrupt();

    result_t kern_result = set_port_kernel(KERNEL_MSG_INT_START+cpu_int_vector, dest_pid, chan);
    if (kern_result != SUCCESS) return 0;

    bool result = program_ioapic(cpu_int_vector, intno);
    return result ? cpu_int_vector : 0;
}

uint8_t configure_interrupts_for(DEVICESD_MESSAGE_REG_INT* m)
{
    if (m->flags & MSG_REG_INT_FLAG_EXTERNAL_INTS) {
        return get_ioapic_int(m->intno, m->dest_task, m->dest_chan);
    }

    // Not supported yet
    return 0;
}