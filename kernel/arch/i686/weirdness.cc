#include <processes/tasks.hh>

kresult_t kernel::proc::TaskDescriptor::set_32bit()
{
    // NOOP
    return 0;
}

bool kernel::proc::TaskDescriptor::is_32bit() const
{
    return true;
}