#ifndef _STDDEF_H
#define _STDDEF_H

#define __DECLARE_WCHAR_T
#define __DECLARE_NULL
#define __DECLARE_SIZE_T
#define __DECLARE_PTRDIFF_T
#include "__posix_types.h"

typedef unsigned long max_align_t;

// This is complicated for C++; use builtin instead
//#define offsetof(TYPE, MEMBER_DESIGNATOR) ((size_t)&((*const TYPE)(0)->MEMBER_DESIGNATOR))
#define offsetof(TYPE, MEMBER_DESIGNATOR) __builtin_offsetof(TYPE, MEMBER_DESIGNATOR)


#endif /* _STDDEF_H */