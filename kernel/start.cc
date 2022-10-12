#include "common/com.h"
#include "gdt.hh"
#include "utils.hh"
#include "misc.hh"

extern "C" int _start(Kernel_Entry_Data* d)
{
    init_gdt();

    t_print("Free after kernel %h\n", unoccupied);

    return 0xdeadc0de;
}