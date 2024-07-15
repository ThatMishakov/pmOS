#include <time.h>
#include <pmos/system.h>
#include <errno.h>
#include <stdio.h>

int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    uint64_t clock_time = 0;

    switch (clk_id) {
        case CLOCK_REALTIME:
            syscall_r t = pmos_get_time(GET_TIME_REALTIME_NANOSECONDS);
            if (t.result != 0) {
                errno = t.result;
                return -1;
            }

            tp->tv_nsec = t.value % 1000000000;
            tp->tv_sec  = t.value / 1000000000;
            return 0;
        case CLOCK_PROCESS_CPUTIME_ID:
            // Not implemented
            fprintf(stderr, "pmOS libc: clock_gettime: CLOCK_PROCESS_CPUTIME_ID not implemented\n");
            errno = ENOSYS;
            return -1;
        case CLOCK_THREAD_CPUTIME_ID:
            // Not implemented
            fprintf(stderr, "pmOS libc: clock_gettime: CLOCK_THREAD_CPUTIME_ID not implemented\n");
            errno = ENOSYS;
            return -1;
        case CLOCK_MONOTONIC: {
            syscall_r t = pmos_get_time(GET_TIME_NANOSECONDS_SINCE_BOOTUP);
            if (t.result != 0) {
                errno = t.result;
                return -1;
            }

            tp->tv_nsec = t.value % 1000000000;
            tp->tv_sec  = t.value / 1000000000;
            return 0;
        }
        default:
            fprintf(stderr, "pmOS libc: clock_gettime: Unknown clock ID\n");
            errno = EINVAL;
            return -1;
    }
}