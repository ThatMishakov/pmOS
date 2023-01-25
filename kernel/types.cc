#include "types.hh"
#include "utils.hh"

void Spinlock::lock()
{
    int counter = 0;
    bool result;
    do {
        result = __sync_bool_compare_and_swap(&locked, false, true);
        if (counter++ == 1000000) {
            print_stack_trace();
            while (1) ;
        }
    } while (not result);
    __sync_synchronize();
}