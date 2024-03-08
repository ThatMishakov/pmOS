#include <kern_logger/kern_logger.hh>
#include <cpus/csr.hh>
#include <sched/sched.hh>

extern "C" void handle_interrupt();

struct fp_s {
    u64 fp;
    u64 ra;
};

void print_stack_trace()
{
    u64 fp = get_cpu_struct()->current_task->regs.s0;
    fp_s *current = (fp_s*)fp - 1;
    serial_logger.printf("Stack trace:\n");
    while (1) {
        serial_logger.printf("  0x%x\n", current->ra);
        if (current->ra == 0) {
            break;
        }
        current = (fp_s*)current->fp - 1;    
    }
}


void handle_interrupt()
{
    u64 scause, stval;
    get_scause_stval(&scause, &stval);

    serial_logger.printf("Recieved an interrupt! scause: 0x%x stval: 0x%x pc 0x%x\n", scause, stval, get_cpu_struct()->current_task->regs.program_counter());

    auto c = get_cpu_struct();
    if (c->nested_level > 1) {
        serial_logger.printf("!!! kernel interrupt !!!\n");
        print_stack_trace();
    }

    while (1) ;
}