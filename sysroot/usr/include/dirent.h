/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __DIRENT_H
#define __DIRENT_H

#define __DECLARE_INO_T
#define __DECLARE_SSIZE_T
#define __DECLARE_OFF_T
#define __DECLARE_SIZE_T
#include "__posix_types.h"

typedef struct {
    unsigned long __fd;
    unsigned long __pos;
} DIR;

#define DT_BLK 06
#define DT_CHR 02
#define DT_DIR 04
#define DT_FIFO 01
#define DT_LNK 012
#define DT_REG 010
#define DT_SOCK 014
#define DT_UNKNOWN 0

struct dirent {
    ino_t  d_ino; //< File serial number.
    off_t   d_off; //< Offset to the next directory entry.
    unsigned short d_reclen; //< Length of this record.
    unsigned short d_type; //< Type of file; not supported by all filesystem types.
    unsigned short d_namlen; //< Length of string in d_name.
    char   d_name[2048]; //< Null-terminated filename of length d_namlen. Maximum of NAME_MAX + 1 bytes.
};



#if defined(__cplusplus)
extern "C" {
#endif

#ifdef __STDC_HOSTED__

int            alphasort(const struct dirent **, const struct dirent **);
int            closedir(DIR *);
int            dirfd(DIR *);
DIR           *fdopendir(int);
DIR           *opendir(const char *);
struct dirent *readdir(DIR *);
int            readdir_r(DIR *, struct dirent *, struct dirent **);
void           rewinddir(DIR *);
int            scandir(const char *, struct dirent ***,
                   int (*)(const struct dirent *),
                   int (*)(const struct dirent **,
                   const struct dirent **));
void           seekdir(DIR *, long int);
long int       telldir(DIR *);

/**
 * @brief Get directory entries in a specified format.
 *
 * The `getdents` function is used to retrieve directory entries from the directory
 * referenced by the file descriptor `fd` into the buffer pointed to by `buf`, in
 * a format independent of the underlying file system. Up to `nbytes` bytes of data
 * are placed into the buffer. The `getdents` function returns the number of bytes
 * placed into the buffer. The file position is advanced by the number of bytes
 * returned. The entries might be separated by holes, which are indicated by extra
 * space in the buffer between entries. d_reclen may be used to determine the location
 * of the next entry, indicating the offset from the start of the dirent structure.
 *
 * @param fd       The file descriptor referring to the open directory.
 * @param buf      A pointer to a buffer where directory entries will be stored.
 * @param nbytes   The size of the buffer pointed to by `buf` in bytes.
 * @return         If successful, the function returns the number of bytes read.
 *                 On reaching the end of the directory or an error, it returns -1
 *                 and sets `errno` to indicate the error.
 *
 * @note           The `getdents` function is not specified in POSIX, but is available
 *                 on many Unix-like systems. Thus the exact format of directory entries
 *                 and the behavior may vary on different systems.
 * 
 * @todo           This function seems inconvenient having in mind how directory tree
 *                 is stored in VFS daemon. Revisit it and consider adding a different
 *                 function.
 * 
 * @todo           getdents_posix() seems to be in a works. Consider adding it.
 */

ssize_t getdents(int fd, char *buf, size_t nbytes);

/**
 * @brief Get directory entries in a specified format.
 *
 * The `getdirentries` function is used to retrieve directory entries from the directory
 * referenced by the file descriptor `fd` into the buffer pointed to by `buf`, in
 * a format independent of the underlying file system. Up to `nbytes` bytes of data
 * are placed into the buffer. The `getdirentries` function returns the number of bytes
 * placed into the buffer. The file position is advanced by the number of bytes
 * returned. The entries might be separated by holes, which are indicated by extra
 * space in the buffer between entries. d_reclen may be used to determine the location
 * of the next entry, indicating the offset from the start of the dirent structure.
 *
 * @param fd       The file descriptor referring to the open directory.
 * @param buf      A pointer to a buffer where directory entries will be stored.
 * @param nbytes   The size of the buffer pointed to by `buf` in bytes.
 * @param basep    A pointer to an offset that keeps track of the current directory
 *                 entry being read. You should initialize it to 0 before the first
 *                 call and pass the same pointer in subsequent calls to continue
 *                 reading entries.
 * @return         If successful, the function returns the number of bytes read.
 *                 On reaching the end of the directory or an error, it returns 0
 *                 and sets `errno` to indicate the error.
 *
 * @note           The `getdirentries` function is not standardized in POSIX, thus
 *                 the exact format of directory entries and the behavior may vary
 *                 on different systems.
 */
ssize_t getdirentries(int fd, char *buf, size_t nbytes, off_t *basep);


#endif // __STDC_HOSTED__

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* __DIRENT_H */