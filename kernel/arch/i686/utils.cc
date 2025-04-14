#include <sched/sched.hh>

static kernel::sched::CPU_Info __seg_gs *c = nullptr;

kernel::sched::CPU_Info *kernel::sched::get_cpu_struct() { return c->self; }