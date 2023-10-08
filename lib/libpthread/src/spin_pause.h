#ifndef SPIN_PAUSE_H
#define SPIN_PAUSE_H
#include <immintrin.h>

inline static void spin_pause() {
    __builtin_ia32_pause();
}

#endif // SPIN_PAUSE_H