/*
 * aarch64/dbga.S
 *
 * Copyright (C) 2021 bzt (bztsrc@github)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * @brief Assembly routines for the mini debugger
 */

.global dbg_init
.global dbg_saveregs
.global dbg_loadregs
.extern dbg_regs
.extern dbg_decodeexc
.extern dbg_main

/**
 * Set up dummy exception handlers to call the mini debugger
 */
dbg_init:
    ldr     x0, =dbg_vectors
    msr     vbar_el1, x0
    ret

/**
 * Save registers before we call any C code
 */
dbg_saveregs:
    str     x0, [sp, #-16]!     /* push x0 */
    ldr     x0, =dbg_regs+8
    str     x1, [x0], #8        /* dbg_regs[1]=x1 */
    ldr     x1, [sp, #16]       /* pop x1 */
    str     x1, [x0, #-16]!     /* dbg_regs[0]=x1 (x0) */
    add     x0, x0, #16
    str     x2, [x0], #8        /* dbg_regs[2]=x2 */
    str     x3, [x0], #8        /* ...etc. */
    str     x4, [x0], #8
    str     x5, [x0], #8
    str     x6, [x0], #8
    str     x7, [x0], #8
    str     x8, [x0], #8
    str     x9, [x0], #8
    str     x10, [x0], #8
    str     x11, [x0], #8
    str     x12, [x0], #8
    str     x13, [x0], #8
    str     x14, [x0], #8
    str     x15, [x0], #8
    str     x16, [x0], #8
    str     x17, [x0], #8
    str     x18, [x0], #8
    str     x19, [x0], #8
    str     x20, [x0], #8
    str     x21, [x0], #8
    str     x22, [x0], #8
    str     x23, [x0], #8
    str     x24, [x0], #8
    str     x25, [x0], #8
    str     x26, [x0], #8
    str     x27, [x0], #8
    str     x28, [x0], #8
    str     x29, [x0], #8
    ldr     x1, [sp, #16]       /* pop x30 */
    str     x1, [x0], #8
    mrs     x1, elr_el1
    str     x1, [x0], #8
    mrs     x1, spsr_el1
    str     x1, [x0], #8
    mrs     x1, esr_el1
    str     x1, [x0], #8
    mrs     x1, far_el1
    str     x1, [x0], #8
    mrs     x1, sctlr_el1
    str     x1, [x0], #8
    mrs     x1, tcr_el1
    str     x1, [x0], #8
    ret

/**
 * Restore registers after we call C code
 */
dbg_loadregs:
    ldr     x0, =dbg_regs+8
    ldr     x1, [x0], #8
    ldr     x2, [x0], #8
    ldr     x3, [x0], #8
    ldr     x4, [x0], #8
    ldr     x5, [x0], #8
    ldr     x6, [x0], #8
    ldr     x7, [x0], #8
    ldr     x8, [x0], #8
    ldr     x9, [x0], #8
    ldr     x10, [x0], #8
    ldr     x11, [x0], #8
    ldr     x12, [x0], #8
    ldr     x13, [x0], #8
    ldr     x14, [x0], #8
    ldr     x15, [x0], #8
    ldr     x16, [x0], #8
    ldr     x17, [x0], #8
    ldr     x18, [x0], #8
    ldr     x19, [x0], #8
    ldr     x20, [x0], #8
    ldr     x21, [x0], #8
    ldr     x22, [x0], #8
    ldr     x23, [x0], #8
    ldr     x24, [x0], #8
    ldr     x25, [x0], #8
    ldr     x26, [x0], #8
    ldr     x27, [x0], #8
    ldr     x28, [x0], #8
    ldr     x29, [x0], #8
    ret

    .align 11
dbg_vectors:
    .align  7
    str     x30, [sp, #-16]!     /* push x30 */
    bl      dbg_saveregs
    mov     x0, #0
    bl      dbg_main
    bl      dbg_loadregs
    ldr     x30, [x0], #8       /* must be set outside of a function call */
    ldr     x0, [x0]            /* update the return address */
    msr     elr_el1, x0
    ldr     x0, =dbg_regs       /* restore x0 as last */
    ldr     x0, [x0]
    eret

    .align  7
    str     x30, [sp, #-16]!
    bl      dbg_saveregs
    mov     x0, #1
    bl      dbg_main
    bl      dbg_loadregs
    ldr     x30, [x0], #8
    ldr     x0, [x0]
    msr     elr_el1, x0
    ldr     x0, =dbg_regs
    ldr     x0, [x0]
    eret

    .align  7
    str     x30, [sp, #-16]!
    bl      dbg_saveregs
    mov     x0, #2
    bl      dbg_main
    bl      dbg_loadregs
    ldr     x30, [x0], #8
    ldr     x0, [x0]
    msr     elr_el1, x0
    ldr     x0, =dbg_regs
    ldr     x0, [x0]
    eret

    .align  7
    str     x30, [sp, #-16]!
    bl      dbg_saveregs
    mov     x0, #3
    bl      dbg_main
    bl      dbg_loadregs
    ldr     x30, [x0], #8
    ldr     x0, [x0]
    msr     elr_el1, x0
    ldr     x0, =dbg_regs
    ldr     x0, [x0]
    eret
