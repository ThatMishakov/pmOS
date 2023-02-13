#include <configuration.h>
#include <ioapic/ioapic.h>
#include <pmos/ipc.h>
#include <interrupts/interrupts.h>
#include <stdbool.h>
#include <pmos/system.h>
#include <pmos/interrupts.h>

uint8_t get_ioapic_int(uint32_t intno, uint64_t dest_pid, uint64_t chan)
{
    uint8_t cpu_int_vector = get_free_interrupt();

    //result_t kern_result = set_port_kernel(KERNEL_MSG_INT_START+cpu_int_vector, dest_pid, chan);
    result_t kern_result = set_interrupt(chan, cpu_int_vector, 0);
    if (kern_result != SUCCESS) return 0;

    bool result = program_ioapic(cpu_int_vector, intno);
    return result ? cpu_int_vector : 0;
}

uint8_t configure_interrupts_for(IPC_Reg_Int* m)
{
    if (m->flags & IPC_Reg_Int_FLAG_EXT_INTS) {
        return get_ioapic_int(m->intno, m->dest_task, m->dest_chan);
    }

    // Not supported yet
    return 0;
}