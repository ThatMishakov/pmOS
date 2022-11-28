#ifndef _STDDEF_H
#define _STDDEF_H
#include "stdlib_com.h"

typedef size_t ptrdiff_t;
typedef unsigned long long max_align_t;
typedef short wchar_t;

#define offsetof(type, member_designator) \
        ((size_t)&((*type)(0)->member_designator))

#endif /* _STDDEF_H */