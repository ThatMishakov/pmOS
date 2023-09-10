#ifndef _GRP_H
#define _GRP_H 1

#define __DECLARE_GID_T
#define __DECLARE_SIZE_T
#include "__posix_types.h"

struct group {
    char *gr_name; //< Group name.
    gid_t gr_gid; //< Numerical group ID.
    char **gr_mem; //< Pointer to a null-terminated array of character pointers to member names.
};

#ifdef __STDC_HOSTED__

#ifdef __cplusplus
extern "C" {
#endif

void           endgrent(void);
struct group  *getgrent(void);
struct group  *getgrgid(gid_t);
int            getgrgid_r(gid_t, struct group *, char *,
                   size_t, struct group **);
struct group  *getgrnam(const char *);
int            getgrnam_r(const char *, struct group *, char *,
                   size_t , struct group **);
void           setgrent(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __STDC_HOSTED__

#endif // _GRP_H
