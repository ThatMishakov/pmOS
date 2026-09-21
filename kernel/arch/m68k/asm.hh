#pragma once

static inline void flush_i_d()
{
    asm("movec %%cacr, %%d0\n\t"
        "oriw %0,%%d0\n\t"
        "movec %%d0,%%cacr"
        :
        : "i"(0x808)
        : "d0", "memory");
}

static inline void flush_m68030(void *virt_addr)
{
    register void *a0 asm("%a0") = virt_addr;

    asm(".word 0xf010, 0x0810\n\t" // pflush #7, #7, (A0)
        "nop\n\t"
        :
        : "r"(a0)
        : "memory");
}

static inline void m68030_flush_user_page(void *uva)
{
    __asm__ volatile (
        "pflush #1,#3,(%0)\n\t"
        "nop\n\t"
        :
        : "a" (uva)
        : "memory"
    );
}

static inline void m68030_flush_user_all()
{
    __asm__ volatile (
        "pflush #1,#3\n\t"
        "nop\n\t"
        :
        :
        : "memory"
    );
}
