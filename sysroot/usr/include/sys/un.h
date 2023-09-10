#ifndef _SYS_UN_H
#define _SYS_UN_H 1

#define __DECLARE_SA_FAMILY_T
#include "__posix_types.h"

struct sockaddr_un {
    sa_family_t sun_family; //< Address family.
    char sun_path[256]; //< Pathname.
};

#endif // _SYS_UN_H