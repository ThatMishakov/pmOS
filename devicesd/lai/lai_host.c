#include <acpi/acpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <asm.h>
#include <phys_map/phys_map.h>
#include <pmos/debug.h>

void laihost_log(int level, const char *msg)
{
    printf("LAI Log level %i: %s", level, msg);
}

__attribute__((noreturn)) void laihost_panic(const char *msg)
{
    fprintf(stderr, "LAI panic: %s", msg);
    pmos_print_stack_trace();
    exit(2);
}

void *laihost_malloc(size_t s)
{
    return malloc(s);
}

void *laihost_realloc(void * p, size_t s)
{
    return realloc(p, s);
}

void laihost_free(void * p)
{
    free(p);
}


/* Maps count bytes from the given physical address and returns
   a virtual address that can be used to access the memory. */
void *laihost_map(size_t address, size_t count)
{
    return map_phys((void*)address, count);
}

/* Unmaps count bytes from the given virtual address.
   LAI only calls this on memory that was previously mapped by laihost_map(). */
void laihost_unmap(void *pointer, size_t count)
{
    unmap_phys(pointer, count);
}



/* Write a byte/word/dword to the given I/O port. */
void laihost_outb(uint16_t port, uint8_t val)
{
    outb(port, val);
}

void laihost_outw(uint16_t port, uint16_t val)
{
    outw(port, val);
}

void laihost_outd(uint16_t port, uint32_t val)
{
    outl(port, val);
}

/* Read a byte/word/dword from the given I/O port. */
uint8_t laihost_inb(uint16_t port)
{
    return inb(port);
}

uint16_t laihost_inw(uint16_t port)
{
    return inw(port);
}

uint32_t laihost_ind(uint16_t port)
{
    return inl(port);
}

void *laihost_scan(char *sig, size_t index)
{
    ACPISDTHeader* h = get_table(sig, index);
    return h;
}