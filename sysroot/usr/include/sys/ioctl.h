#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H 1

#define TIOCGWINSZ 0x0 // Get window size.
#define TIOCSWINSZ 0x1 // Set window size.

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_HOSTED__

int ioctl(int fd, unsigned long request, ...);

#endif // __STDC_HOSTED__

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _SYS_IOCTL_H
