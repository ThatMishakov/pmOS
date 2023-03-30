#ifndef _ASSERT_H
#define _ASSERT_H

#ifdef __STDC_HOSTED__
#if defined(__cplusplus)
extern "C" {
#endif
void _assert_fail(const char* condition, const char* file, unsigned int line);
#if defined(__cplusplus)
} /* extern "C" */
#endif
#endif

#ifdef NDEBUG
#define assert(bool)
#else
#define assert(COND) do {if (!(COND)) _assert_fail(#COND, __FILE__, __LINE__);} while (0) 
#endif


#endif /* _ASSERT_H */