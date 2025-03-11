#include <processes/tasks.hh>

bool TaskDescriptor::is_32bit() const {
    return false;
}

kresult_t TaskDescriptor::set_32bit()
{
    // NOOP
    return 0;
}