#ifndef _STDDEF_H
#define _STDDEF_H

#define __DECLARE_WCHAR_T
#define __DECLARE_NULL
#define __DECLARE_SIZE_T
#define __DECLARE_NULL
#include "__posix_types.h"

typedef size_t ptrdiff_t;
typedef unsigned long long max_align_t;

#define offsetof(type, member_designator) \
        ((size_t)&((*type)(0)->member_designator))

#endif /* _STDDEF_H */