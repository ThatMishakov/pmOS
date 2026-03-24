#include <assert.h>
#include <pmos/memory.h>
#include <pmos/memory_flags.h>
#include <pmos/tls.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

void __init_uthread(struct uthread *u, void *stack_top, size_t stack_size);

#define max(x, y)                ((x) > (y) ? (x) : (y))
#define alignup(size, alignment) (size % alignment ? size + (alignment - size % alignment) : size)

void *__get_tp()
{
    void *out;
    __asm__ ("move %0, $tp" : "=r"(out) );
    return out;
}