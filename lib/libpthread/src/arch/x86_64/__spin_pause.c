#include <immintrin.h>

void __spin_pause(void)
{
    __builtin_ia32_pause();
}