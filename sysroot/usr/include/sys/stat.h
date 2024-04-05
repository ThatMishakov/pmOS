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

#ifndef __SYS_STAT_H
#define __SYS_STAT_H
#include <time.h>

#define __DECLARE_BLKCNT_T
#define __DECLARE_BLKSIZE_T
#define __DECLARE_DEV_T
#define __DECLARE_INO_T
#define __DECLARE_MODE_T
#define __DECLARE_NLINK_T
#define __DECLARE_UID_T
#define __DECLARE_GID_T
#define __DECLARE_OFF_T
#define __DECLARE_TIME_T
#define __DECLARE_TIMESPEC_T
#include "../__posix_types.h"

// TODO: File type macros and whatnot

#define S_IFMT 0170000

#define S_IFBLK 0060000
#define S_IFCHR 0020000
#define S_IFIFO 0010000
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_IFLNK 0120000
#define S_IFSOCK 0140000

#define S_ISBLK(m) ((m & S_IFMT) == S_IFBLK)
#define S_ISCHR(m) ((m & S_IFMT) == S_IFCHR)
#define S_ISDIR(m) ((m & S_IFMT) == S_IFDIR)
#define S_ISFIFO(m) ((m & S_IFMT) == S_IFIFO)
#define S_ISREG(m) ((m & S_IFMT) == S_IFREG)
#define S_ISLNK(m) ((m & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) ((m & S_IFMT) == S_IFSOCK)

#define S_TYPEISMQ(buf) 0 // TODO
#define S_TYPEISSEM(buf) 0
#define S_TYPEISSHM(buf) 0
#define S_TYPEISTMO(buf) 0

#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000

#define S_IRWXU 0700
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100

#define S_IRWXG 070
#define S_IRGRP 040
#define S_IWGRP 020
#define S_IXGRP 010

#define S_IRWXO 07
#define S_IROTH 04
#define S_IWOTH 02
#define S_IXOTH 01

struct stat {
    dev_t     st_dev; ///<     Device ID of device containing file. 
    ino_t     st_ino;  ///<   File serial number. 
    mode_t    st_mode; ///<   Mode of file (see below). 
    nlink_t   st_nlink;///<   Number of hard links to the file. 
    uid_t     st_uid; ///<     User ID of file. 
    gid_t     st_gid; ///<     Group ID of file. 
    dev_t     st_rdev; ///<    Device ID (if file is character or block special). 
    off_t     st_size; ///<    For regular files, the file size in bytes. 
                       ///     For symbolic links, the length in bytes of the 
                       ///     pathname contained in the symbolic link. 
                       ///     For a shared memory object, the length in bytes. 
                       ///     For a typed memory object, the length in bytes. 
                       ///     For other file types, the use of this field is 
                       ///     unspecified. 
    struct timespec   st_atim; ///<   Time of last access. 
    struct timespec    st_mtim; ///<   Time of last data modification. 
    struct timespec    st_ctim; ///<   Time of last status change. 
    blksize_t st_blksize; ///< A file system-specific preferred I/O block size for 
                          ///  this object. In some file system types, this may 
                          ///  vary from file to file. 
    blkcnt_t  st_blocks;  ///<  Number of blocks allocated for this object. 
};

#define S_ISBLK(m) ((m))

#if defined(__cplusplus)
extern "C" {
#endif

int    chmod(const char *, mode_t);
int    fchmod(int, mode_t);
int    fchmodat(int, const char *, mode_t, int);
int    fstat(int, struct stat *);
int    fstatat(int, const char *, struct stat *, int);
int    futimens(int, const struct timespec [2]);
int    lstat(const char *, struct stat *);
int    mkdir(const char *, mode_t);
int    mkdirat(int, const char *, mode_t);
int    mkfifo(const char *, mode_t);
int    mkfifoat(int, const char *, mode_t);

int    mknod(const char *, mode_t, dev_t);
int    mknodat(int, const char *, mode_t, dev_t);

int    stat(const char *, struct stat *);
mode_t umask(mode_t);
int    utimensat(int, const char *, const struct timespec [2], int);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif