#ifndef _SPECIAL_H
#define _SPECIAL_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef __STDC_HOSTED__

/**
 * Requests the permission to use the processor's IO ports (in* and out* assembly instructions).
 * 
 * @return int 0 if the request was successfull. -1 if the process does not have enough permissions
 *         to use the command.
 */
int pmos_request_io_permission();

#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* _SPECIAL_H */