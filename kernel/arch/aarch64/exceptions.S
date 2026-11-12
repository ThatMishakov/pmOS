.section .text
.balign 2048
.global exception_vectors
exception_vectors:
    // Current EL, SP_EL0
    b user_sync
    .balign 128
    b user_irq
    .balign 128
    b user_fiq
    .balign 128
    b user_serror

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
    b user_sync
    .balign 128
    b user_irq
    .balign 128
    b user_fiq
    .balign 128
    b user_serror
 
    // Lower EL, AArch32
    .balign 128
    b unexpected_exception
    b user_sync
    .balign 128
    b user_irq
    .balign 128
    b user_fiq
    .balign 128
    b user_serror
    
    .macro save_user_state
    str x0, [sp, #-8]!
    mrs x0, TPIDR_EL1
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

    mrs x1, ELR_EL1
    mrs x2, SP_EL0
    mrs x3, SPSR_EL1
    mrs x4, TPIDR_EL0
    mrs x5, TPIDRRO_EL0

    stp x1, x2, [x0, #248]
    stp x3, x4, [x0, #264]
    str x5, [x0, #280]
    .endm


    .type user_sync, @function
user_sync:
    save_user_state

    bl handle_user_sync

    b return_to_user

    .type user_irq, @function
user_irq:
    save_user_state

    bl handle_user_irq

    b return_to_user

    .type user_fiq, @function
user_fiq:
    save_user_state
    bl handle_user_fiq
    b return_to_user

    .type user_serror, @function
user_serror:
    save_user_state
    bl handle_user_serror
    b return_to_user


    .globl return_to_user
    .type return_to_user, @function
return_to_user:
    mrs x0, TPIDR_EL1
    ldr x0, [x0, #32]

    ldp x1, x2, [x0, #248]
    ldp x3, x4, [x0, #264]
    ldr x5, [x0, #280]
    msr ELR_EL1, x1
    msr SP_EL0, x2
    msr SPSR_EL1, x3
    msr TPIDR_EL0, x4
    msr TPIDRRO_EL0, x5

    ldp x2, x3, [x0, #16]
    ldp x4, x5, [x0, #32]
    ldp x6, x7, [x0, #48]
    ldp x8, x9, [x0, #64]
    ldp x10, x11, [x0, #80]
    ldp x12, x13, [x0, #96]
    ldp x14, x15, [x0, #112]
    ldp x16, x17, [x0, #128]
    ldp x18, x19, [x0, #144]
    ldp x20, x21, [x0, #160]
    ldp x22, x23, [x0, #176]
    ldp x24, x25, [x0, #192]
    ldp x26, x27, [x0, #208]
    ldp x28, x29, [x0, #224]
    ldr x30, [x0, #240]
    ldp x0, x1, [x0]
    eret