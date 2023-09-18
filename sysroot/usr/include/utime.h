#ifndef __UTIME_H
#define __UTIME_H

#define __DECLARE_TIME_T
#include "__posix_types.h"

struct utimbuf {
    time_t    actime;
    time_t    modtime;
};



#if defined(__cplusplus)
extern "C" {
#endif

int utime(const char *, const struct utimbuf *);


#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* __UTIME_H */