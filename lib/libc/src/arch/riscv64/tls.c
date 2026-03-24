#include <assert.h>
#include <pmos/memory.h>
#include <pmos/memory_flags.h>
#include <pmos/tls.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>

void __init_uthread(struct uthread *u, void *stack_top, size_t stack_size);

#define alignup(size, alignment) (size % alignment ? size + (alignment - size % alignment) : size)