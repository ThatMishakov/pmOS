#include <acpi.h>
#include <multiboot2.h>
#include <io.h>
#include <syscall.h>

extern int started_cpus;
void (*kernel_cpu_init)(void);

void start_cpus()
{
    // TODO: This is very ugly & ideally should be moved to the kernel
    extern char _cpuinit_start;
    extern char _cpuinit_end;
    
    kernel_cpu_init = (void*)syscall(SYSCALL_CONFIGURE_SYSTEM, 3, 0, 0).value;
    lprintf("CPUINIT %h %h %h\n", &_cpuinit_start, &_cpuinit_end, kernel_cpu_init);

    lprintf("Bringing up CPU...\n");
    syscall(SYSCALL_CONFIGURE_SYSTEM, 2, 1, 0);
    //for (int i = 0; i < 100000; ++i)
    //    asm volatile ("");

    syscall(SYSCALL_CONFIGURE_SYSTEM, 4, 50, 0);

    uint32_t vector = (uint32_t)(&_cpuinit_start) >> 12;
    syscall(SYSCALL_CONFIGURE_SYSTEM, 2, 2, vector);

    // TODO: Second SIPI

    for (int i = 0; i < 20 && !started_cpus; ++i)
         syscall(SYSCALL_CONFIGURE_SYSTEM, 4, 100, 0);

    if (!start_cpus)
        syscall(SYSCALL_CONFIGURE_SYSTEM, 2, 2, vector);


    // This makes no sense
    // syscall(SYSCALL_CONFIGURE_SYSTEM, 2, 3, 2 | (50 << 8));

    // syscall(SYSCALL_CONFIGURE_SYSTEM, 2, 3, 2 | (100 << 8));

    // TODO: Put a tangent about programming languages here
}