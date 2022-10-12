#include "common/com.h"
#include "gdt.hh"
#include "utils.hh"

extern "C" int _start(Kernel_Entry_Data* d)
{
    init_gdt();

    return 0xdeadc0de;
}