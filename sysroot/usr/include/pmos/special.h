#ifndef _SPECIAL_H
#define _SPECIAL_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "../stdlib_com.h"

// Returns 0 if successfull or -1 if process doesn't have enough permissions.
int pmos_request_io_permission();

// Reserves given number of pages and returns (page-alligned) pointer to them
void* heap_reserve_pages(size_t nb_pages);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* _SPECIAL_H */