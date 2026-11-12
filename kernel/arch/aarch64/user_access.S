    .text
    .type fast_atomic_read_from_user, @function
    .globl fast_atomic_read_from_user
fast_atomic_read_from_user:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    stp x27, x28, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x19, x20, [sp, #-16]!

    # Args
    stp x0, x1, [sp, #-16]! // uint32_t *to, const uint32_t *from

    adr x10, read_on_failure
    mrs x2, TPIDR_EL1
    mov x11, sp
    stp x10, x11, [x2, #48] // CPU_Info::jumpto_func, CPU_Info::jumpto_arg

    adrp x3, use_pan
    ldrb w3, [x3, :lo12:use_pan]
    cbz w3, 1f
    msr pan, #0
1:

    ldar w4, [x1]

    cbz w3, 2f
    msr pan, #1
2:

    str w4, [x0]

    // Success
    mov x0, xzr
    mov x1, #1

    stp xzr, xzr, [x2, #48] // CPU_Info::jumpto_func, CPU_Info::jumpto_arg
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret

    .type read_on_failure, @function
read_on_failure:
    mov sp, x0
    add x29, sp, #96

    // Pass arguments
    mov x0, #1 // unsigned access
    // faulting address in x1
    mov x2, xzr // void *&dst = nullptr
    mov x3, xzr // const void *src = nullptr
    mov x4, xzr // size_t *size = 0

    bl user_access_page_fault

    cbnz x0, done
    tbz w1, #0, done

    ldp x0, x1, [sp]

    adrp x3, use_pan
    ldrb w3, [x3, :lo12:use_pan]
    cbz w3, 1f
    msr pan, #0
1:

    ldar w4, [x1]

    cbz w3, 2f
    msr pan, #1
2:

    str w4, [x0]

    # Success
    mov x0, xzr
    mov x1, #1

done:
    mrs x2, TPIDR_EL1
    stp xzr, xzr, [x2, #48] // CPU_Info::jumpto_func, CPU_Info::jumpto_arg

    ldp x19, x20, [x29, #-80]
    ldp x21, x22, [x29, #-64]
    ldp x23, x24, [x29, #-48]
    ldp x25, x26, [x29, #-32]
    ldp x27, x28, [x29, #-16]
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret