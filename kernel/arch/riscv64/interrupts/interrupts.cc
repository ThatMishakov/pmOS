#include <kern_logger/kern_logger.hh>
#include <cpus/csr.hh>

extern "C" void handle_interrupt();

void handle_interrupt()
{
    u64 scause, stval;
    get_scause_stval(&scause, &stval);

    serial_logger.printf("Recieved an interrupt! scause: 0x%x stval: 0x%x\n", scause, stval);
    while (1) ;
}