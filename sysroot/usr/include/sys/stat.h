#ifndef __SYS_STAT_H
#define __SYS_STAT_H
#include "types.h"

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
    time_t    st_atime; ///<   Time of last access. 
    time_t    st_mtime; ///<   Time of last data modification. 
    time_t    st_ctime; ///<   Time of last status change. 
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
int    fstat(int, struct stat *);
int    lstat(const char *, struct stat *);
int    mkdir(const char *, mode_t);
int    mkfifo(const char *, mode_t);
int    stat(const char *, struct stat *);
mode_t umask(mode_t);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif