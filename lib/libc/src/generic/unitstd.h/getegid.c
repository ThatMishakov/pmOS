#include <unistd.h>

uid_t getegid(void)
{
    // At the moment, there are no users in the system
    return 0;
}