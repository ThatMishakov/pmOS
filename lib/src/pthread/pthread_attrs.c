#include <pthread.h>
#include <errno.h>

int pthread_attr_destroy(pthread_attr_t *)
{
    return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t * attrs_ptr, int * detachstate)
{
    if (!attrs_ptr || !detachstate) {
        errno = EINVAL;
        return -1;
    }

    *detachstate = attrs_ptr->detachstate;
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t * attr, int detachstate)
{
    if (!attr) {
        errno = EINVAL;
        return -1;
    }

    switch (detachstate) {
    case PTHREAD_CREATE_JOINABLE:
    case PTHREAD_CREATE_DETACHED:
        attr->detachstate = detachstate;
        break;
    default:
        // errno = EINVAL;
        return -1;
        break;
    }

    return 0;
}

int   pthread_attr_setguardsize(pthread_attr_t * attr, size_t guardsize)
{
    if (!attr || !guardsize) {
        errno = EINVAL;
        return -1;
    }

    attr->guardsize = guardsize;
    return 0;
}

int pthread_attr_getguardsize(const pthread_attr_t * attr, size_t * guardsize)
{
    if (!attr || !guardsize) {
        errno = EINVAL;
        return -1;
    }

    *guardsize = attr->guardsize;
    return 0;
}