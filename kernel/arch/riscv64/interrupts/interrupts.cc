#include <kern_logger/kern_logger.hh>

extern "C" void handle_interrupt();

void handle_interrupt()
{
    serial_logger.printf("Recieved and interrupt!");
    while (1) ;
}