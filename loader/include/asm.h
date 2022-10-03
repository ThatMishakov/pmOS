#ifndef ASM_H
#define ASM_H

#define ENTRY(name) \
  .globl name; \
    .type name, @function; \
    .align 0; \
  name:

#endif