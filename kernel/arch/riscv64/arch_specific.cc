#include <types.hh>

void halt() { asm volatile("wfi"); }

void dma_flush(void *, size_t)
{}