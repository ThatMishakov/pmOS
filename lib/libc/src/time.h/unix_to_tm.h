#include <stdbool.h>
#include <time.h>

/// @brief Convert unix time to a tm struct
///
/// This function converts unix time to a tm struct. It is used by localtime(), gmtime() and other time functions
/// to convert the time to a human readable format. The isdst parameter is passed to the tm struct. Both positive
/// and negative values are supported.
///
/// Years before 1900 and after 9999 are not supported. If the year is out of range, errno is set to ERANGE.
/// @param unix_time Unix time in seconds since the epoch (Jan 1, 1970)
/// @param out Pointer to a tm struct where the result will be stored
/// @param isdst Daylight savings time flag
/// @return 0 on success, -1 on failure. If an error occurs, errno is set.
int __unix_time_to_tm(time_t unix_time, struct tm *out, bool isdst);