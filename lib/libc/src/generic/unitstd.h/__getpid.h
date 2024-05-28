#pragma once
#include <unistd.h>

#define GETPID_PID  1
#define GETPID_PPID 2
#define GETPID_PGID 3

pid_t __attribute__((visibility("hidden"))) __getpid(int type);