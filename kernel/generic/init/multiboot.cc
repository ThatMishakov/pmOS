#if defined(__x86_64__) || defined(__i386__)

#include <multiboot2.h>
#include <types.hh>
#include <paging/arch_paging.hh>
#include <kern_logger/kern_logger.hh>
#include "common.hh"

using namespace kernel;

extern void *_kernel_start;

struct multiboot_info {
    u32 total_size;
    u32 reserved;
};

extern void hcf();

extern "C" void multiboot_main(multiboot_info* info) {
    early_detect_cpu_features();

    log::serial_logger.printf("Hello from pmOS kernel!\n");
    log::serial_logger.printf("Kernel start: 0x%lh\n", &_kernel_start);

    hcf();
}

#endif