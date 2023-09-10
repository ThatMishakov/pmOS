#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H 1

#define __DECLARE_CLOCK_T
#define __DECLARE_TIME_T
#include "__posix_types.h"

struct tms {
    clock_t tms_utime; //< User CPU time.
    clock_t tms_stime; //< System CPU time.
    clock_t tms_cutime; //< User CPU time of terminated child processes.
    clock_t tms_cstime; //< System CPU time of terminated child processes.
};

#ifdef __STDC_HOSTED__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get process times.
 *
 * The `times` function returns the current process times.
 *
 * @param buffer A pointer to a `tms` structure to be filled with the current process times.
 * @return       The `times` function returns the number of clock ticks that have elapsed
 *               since an arbitrary point in the past (for example, system startup); this
 *               point does not change within different invocations of the function.
 *               If an error occurs, the function returns `-1`.
 */
clock_t times(struct tms *buffer);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __STDC_HOSTED__

#endif // _SYS_TIMES_H