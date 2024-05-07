#ifndef _LANGINFO_H
#define _LANGINFO_H

#define __DECLARE_LOCALE_T
#include "__posix_types.h"

#include <nl_types.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define CODESET     0
#define D_T_FMT     1
#define D_FMT       2
#define T_FMT       3
#define T_FMT_AMPM  4
#define AM_STR      5
#define PM_STR      6
#define DAY_1       7
#define DAY_2       8
#define DAY_3       9
#define DAY         10
#define ABDAY_1     11
#define ABDAY_2     12
#define ABDAY_3     13
#define ABDAY_4     14
#define ABDAY_5     15
#define ABDAY_6     16
#define ABDAY_7     17
#define MON_1       18
#define MON_2       19
#define MON_3       20
#define MON_4       21
#define MON_5       22
#define MON_6       23
#define MON_7       24
#define MON_8       25
#define MON_9       26
#define MON_10      27
#define MON_11      28
#define MON_12      29
#define ABMON_1     30
#define ABMON_2     31
#define ABMON_3     32
#define ABMON_4     33
#define ABMON_5     34
#define ABMON_6     35
#define ABMON_7     36
#define ABMON_8     37
#define ABMON_9     38
#define ABMON_10    39
#define ABMON_11    40
#define ABMON_12    41
#define ERA         42
#define ERA_D_FMT   43
#define ERA_D_T_FMT 44
#define ERA_T_FMT   45
#define ALT_DIGITS  46
#define RADIXCHAR   47
#define THOUSEP     48
#define YESEXPR     49
#define NOEXPR      50
#define YESSTR      51
#define NOSTR       52
#define CRNCYSTR    53

#ifdef __STDC_HOSTED__

char *nl_langinfo(nl_item item);

#endif

#if defined(__cplusplus)
}
#endif

#endif