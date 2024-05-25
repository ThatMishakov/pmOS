#pragma once
#include <signal.h>

extern __attribute__ ((visibility ("hidden"))) struct sigaction __sigactions[NSIG];
extern __attribute__ ((visibility ("hidden"))) int __sigaction_lock;