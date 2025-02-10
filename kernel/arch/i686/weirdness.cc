#include <processes/tasks.hh>

kresult_t TaskDescriptor::set_32bit()
{
    // NOOP
    return 0;
}

bool TaskDescriptor::is_32bit() const
{
    return true;
}