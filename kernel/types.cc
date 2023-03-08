#include "types.hh"
#include "utils.hh"
#include <kern_logger/kern_logger.hh>

void Spinlock::lock()
{
    int counter = 0;
    bool result;
    do {
        result = __sync_bool_compare_and_swap(&locked, false, true);
        if (counter++ == 10000000) {
            t_print_bochs("Possible deadlock at spinlock 0x%h... ", this);
            print_stack_trace(bochs_logger);
        }
    } while (not result);
    __sync_synchronize();
}

