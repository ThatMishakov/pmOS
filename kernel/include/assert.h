#ifndef _ASSERT_H
#define _ASSERT_H

#if defined(__cplusplus)
extern "C" {
#endif

void _assert_fail(const char* condition, const char* file, unsigned int line);

#ifdef NDEBUG
#define assert(bool)
#else
#define assert(COND) if (!(COND)) _assert_fail(#COND, __FILE__, __LINE__); 
#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif