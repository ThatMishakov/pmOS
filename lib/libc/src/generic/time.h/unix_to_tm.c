#include <time.h>
#include <errno.h>
#include <stdbool.h>

int __unix_time_to_tm(time_t unix_time, struct tm *out, bool isdst) {
    if (out == NULL) {
        errno = EINVAL; // Invalid argument (out is NULL)
        return -1;
    }

    // Constants for seconds in each unit of time
    const int seconds_in_minute = 60;
    const int seconds_in_hour = 3600;
    const int seconds_in_day = 86400;
    const int days_in_nonleap_year = 365;

    // Days per month (non-leap year)
    const int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // Days per month (leap year)
    const int days_in_month_leap[] = {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // Initialize the result struct tm
    out->tm_isdst = isdst;
    out->tm_sec = unix_time % seconds_in_minute;
    unix_time /= seconds_in_minute;
    out->tm_min = unix_time % seconds_in_minute;
    unix_time /= seconds_in_minute;
    out->tm_hour = unix_time % (seconds_in_hour);
    unix_time /= (seconds_in_hour);

    // Compute year, month, and day using long int
    long int days_since_epoch = unix_time / (seconds_in_day); // Days since epoch (Jan 1, 1970)
    long int year = 1970;

    while (unix_time < 0 || unix_time >= days_in_nonleap_year * seconds_in_day) {
        long int days_in_year = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? days_in_nonleap_year + 1 : days_in_nonleap_year;
        if (unix_time < 0) {
            year--;
            unix_time += days_in_year * seconds_in_day;
        } else {
            year++;
            unix_time -= days_in_year * seconds_in_day;
        }
    }

    // Ensure that tm_year is within a reasonable range
    if (year < 1900 || year > 9999) {
        errno = ERANGE; // Range error (year overflow/underflow)
        return -1;
    }

    out->tm_year = year - 1900; // Years since 1900
    out->tm_yday = (int)days_since_epoch;

    const int* days_in_month_array = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? days_in_month_leap : days_in_month;

    int month = 1;
    while (1) {
        if (days_since_epoch < days_in_month_array[month]) {
            break;
        }
        days_since_epoch -= days_in_month_array[month];
        month++;
    }

    out->tm_mon = month - 1; // Months are 0-based in struct tm
    out->tm_mday = (int)days_since_epoch + 1; // Days are 1-based in struct tm

    return 0; // Success
}