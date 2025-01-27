#include <sched/sched.hh>

CPU_Info *get_cpu_struct()
{
    CPU_Info *ret;
    asm ("movl %%gs:0, %0" : "=r"(ret));
    return ret;
}

