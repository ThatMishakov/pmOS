#include <pmos/tls.h>

struct uthread * __get_tls();

pthread_t pthread_self()
{
    return (pthread_t)__get_tls();
}