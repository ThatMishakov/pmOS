#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H 1

#define _UTSNAME_LENGTH 64

struct utsname {
    char sysname[_UTSNAME_LENGTH]; //< Operating system name.
    char nodename[_UTSNAME_LENGTH]; //< Network node name.
    char release[_UTSNAME_LENGTH]; //< Operating system release.
    char version[_UTSNAME_LENGTH]; //< Operating system version.
    char machine[_UTSNAME_LENGTH]; //< Hardware identifier.
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_HOSTED__

/**
 * @brief Get name and information about current system.
 * 
 * The `uname` function stores information about the current system in the
 * structure pointed to by `buf`. The `buf` argument must point to a structure
 * of type `struct utsname`.
 * 
 * @param buf Pointer to a structure of type `struct utsname`.
 * @return int Zero on success, or `-1` on error.
 */
int uname(struct utsname * buf);

#endif // __STDC_HOSTED__

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _SYS_UTSNAME_H