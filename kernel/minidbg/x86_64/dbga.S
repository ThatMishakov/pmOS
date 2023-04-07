/*
 * x86_64/dbga.S
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

.text

/**
 * Set up dummy exception handlers to call the mini debugger
 */
dbg_init:
    leaq    dbg_idt, %rsi
    leaq    dbg_isrs, %rdi
    /* exception gates */
    movw    $32, %cx
    xorq    %rax, %rax
    movw    %cs, %ax
    orl     $0x0E000000, %eax
    shlq    $16, %rax
    movq    %rax, %rdx
1:  movl    %edi, %eax
    andl    $0xFFFF0000, %eax
    shlq    $48, %rax
    movw    %di, %ax
    orq     %rdx, %rax
    stosq
    movq    %rdi, %rax
    shrq    $32, %rax
    stosq
    addq    $32, %rdi
    decw    %cx
    jnz     1b
    /* interrupt gates */
    xorq    %rax, %rax
    movw    $16, %cx
    orl     $0x0F000000, %eax
    shlq    $16, %rax
    orq     %rax, %rdx
1:  movl    %edi, %eax
    andl    $0xFFFF0000, %eax
    shlq    $48, %rax
    movw    %di, %ax
    orq     %rdx, %rax
    stosq
    movq    %rdi, %rax
    shrq    $32, %rax
    stosq
    addq    $32, %rdi
    decw    %cx
    jnz     1b
    lidt    dbg_idt_value
    ret

/**
 * Save registers before we call any C code
 */
dbg_saveregs:
    movq    %rax, dbg_regs +  0
    movq    %rbx, dbg_regs +  8
    movq    %rcx, dbg_regs + 16
    movq    %rdx, dbg_regs + 24
    movq    %rsi, dbg_regs + 32
    movq    %rdi, dbg_regs + 40
    movq    40(%rsp), %rax          /* orig rsp */
    movq    %rax, dbg_regs + 48
    movq    %rax, dbg_regs + 29*8
    movq    %rbp, dbg_regs + 56
    movq    %r8, dbg_regs + 64
    movq    %r9, dbg_regs + 72
    movq    %r10, dbg_regs + 80
    movq    %r11, dbg_regs + 88
    movq    %r12, dbg_regs + 96
    movq    %r13, dbg_regs + 104
    movq    %r14, dbg_regs + 112
    movq    %r15, dbg_regs + 120
    movq    16(%rsp), %rax          /* orig rip */
    movq    %rax, dbg_regs + 128
    movq    %rax, dbg_regs + 31*8
    movq    %cr0, %rax
    movq    %rax, dbg_regs + 136
    # movq    %cr1, %rax
    # movq    %rax, dbg_regs + 144
    movq    %cr2, %rax
    movq    %rax, dbg_regs + 152
    movq    %cr3, %rax
    movq    %rax, dbg_regs + 160
    movq    %cr4, %rax
    movq    %rax, dbg_regs + 168
    movq    32(%rsp), %rax          /* rflags */
    movq    %rax, dbg_regs + 176
    movq    8(%rsp), %rax           /* exception code */
    movq    %rax, dbg_regs + 184
    ret

/**
 * Restore registers after we call C code
 */
dbg_loadregs:
    movq    dbg_regs +  0, %rax
    movq    dbg_regs +  8, %rbx
    movq    dbg_regs + 16, %rcx
    movq    dbg_regs + 24, %rdx
    movq    dbg_regs + 32, %rsi
    movq    dbg_regs + 40, %rdi
    movq    dbg_regs + 56, %rbp
    movq    dbg_regs + 64, %r8
    movq    dbg_regs + 72, %r9
    movq    dbg_regs + 80, %r10
    movq    dbg_regs + 88, %r11
    movq    dbg_regs + 96, %r12
    movq    dbg_regs + 104, %r13
    movq    dbg_regs + 112, %r14
    movq    dbg_regs + 120, %r15
    ret

.macro isr num, errcode, name, func
    .balign 32
    cli
.if \errcode == 0
    pushq   $0
.endif
    call    dbg_saveregs
    addq    $8, %rsp
    movq    $\num, %rdi
    call    dbg_main
    movq    dbg_regs + 31*8, %rax   /* update the return address */
    movq    %rax, (%rsp)
    call    dbg_loadregs
    iretq
.endm

    .balign 32
dbg_isrs:
    isr  0, 0
    isr  1, 0
    isr  2, 0
    isr  3, 0
    isr  4, 0
    isr  5, 0
    isr  6, 0
    isr  7, 0
    isr  8, 1
    isr  9, 0
    isr 10, 1
    isr 11, 1
    isr 12, 1
    isr 13, 1
    isr 14, 1
    isr 15, 0
    isr 16, 0
    isr 17, 1
    isr 18, 0
    isr 19, 0
    isr 20, 0
    isr 21, 0
    isr 22, 0
    isr 23, 0
    isr 24, 0
    isr 25, 0
    isr 26, 0
    isr 27, 0
    isr 28, 0
    isr 29, 0
    isr 30, 1
    isr 31, 0

    isr 32, 0
    isr 33, 0
    isr 34, 0
    isr 35, 0
    isr 36, 0
    isr 37, 0
    isr 38, 0
    isr 39, 0
    isr 40, 0
    isr 41, 0
    isr 42, 0
    isr 43, 0
    isr 44, 0
    isr 45, 0
    isr 46, 0
    isr 47, 0

.data
    .balign 16
dbg_idt:
    .space (48*16)
dbg_idt_value:
    .word (dbg_idt_value-dbg_idt)-1
    .quad dbg_idt
