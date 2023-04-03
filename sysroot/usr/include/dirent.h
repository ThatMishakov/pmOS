#ifndef __DIRENT_H
#define __DIRENT_H

#define __DECLARE_INO_T
#include "__posix_types.h"

typedef unsigned long DIR;

struct dirent {
    ino_t  d_ino;
    char   d_name[];
};



#if defined(__cplusplus)
extern "C" {
#endif

int            closedir(DIR *);
DIR           *opendir(const char *);
struct dirent *readdir(DIR *);
int            readdir_r(DIR *, struct dirent *, struct dirent **);
void           rewinddir(DIR *);
void           seekdir(DIR *, long int);
long int       telldir(DIR *);


#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* __DIRENT_H */