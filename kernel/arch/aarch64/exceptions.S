.section .text
.balign 2048
.global exception_vectors
exception_vectors:
    // Current EL, SP_EL0
    b unexpected_exception
    .balign 128
    b unexpected_exception
    .balign 128
    b unexpected_exception
    .balign 128
    b unexpected_exception

    // Current EL, SP_EL1
    .balign 128
//    b kernel_sync
    .balign 128
//    b kernel_irq
    .balign 128
//    b kernel_fiq
    .balign 128
//    b kernel_serror

    // Lower EL, AArch64
    .balign 128
//    b user_sync
    .balign 128
//    b user_irq
    .balign 128
//    b user_fiq
    .balign 128
//    b user_serror
 
    // Lower EL, AArch32
    .rept 4
    .balign 128
    b unexpected_exception
    .endr

    .macro save_user_state
    str x0, [sp, #-8]!
    mrs x0, tpidr_el1
    ldr x0, [x0, #32]

    str x1, [x0, #8]
    ldr x1, [sp], #8
    str x1, [x0]
    stp x2, x3, [x0, #16]
    stp x4, x5, [x0, #32]
    stp x6, x7, [x0, #48]
    stp x8, x9, [x0, #64]
    stp x10, x11, [x0, #80]
    stp x12, x13, [x0, #96]
    stp x14, x15, [x0, #112]
    stp x16, x17, [x0, #128]
    stp x18, x19, [x0, #144]
    stp x20, x21, [x0, #160]
    stp x22, x23, [x0, #176]
    stp x24, x25, [x0, #192]
    stp x26, x27, [x0, #208]
    stp x28, x29, [x0, #224]
    str x30, [x0, #240]

    mrs x1, elr_el1
    mrs x2, sp_el0
    mrs x3, spsr_el1
    mrs x4, tpidr_el0
    mrs x5, tpidrro_el0

    stp x1, x2, [x0, #248]
    stp x3, x4, [x0, #264]
    str x5, [x0, #280]
    .endm

    