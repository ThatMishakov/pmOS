#include <sched/sched.hh>

static CPU_Info __seg_gs *c = nullptr;

CPU_Info *get_cpu_struct() { return c->self; }