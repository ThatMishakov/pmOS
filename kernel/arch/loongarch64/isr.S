    .text
    .p2align 12
    .type isr, @function
    .globl isr
isr:
    # Load kernel's tp
    csrwr $tp, 0x30

    # Save t0 (temporary register)
    st.d $t0, $tp, 16

    # Check nested level (if the interrupt is coming from kernel)
    ld.d $t0, $tp, 24
    bne $t0, $zero, nested_interrupt

    # Not nested int
    addi.d $t0, $t0, 1
    st.d $t0, $tp, 24

    # Load user data stucture
    ld.d $t0, $tp, 32

    # Save registers
    st.d $r1, $t0, 8
    # st.d $r2, $t0, 16
    st.d $r3, $t0, 24
    st.d $r4, $t0, 32
    st.d $r5, $t0, 40
    st.d $r6, $t0, 48
    st.d $r7, $t0, 56
    st.d $r8, $t0, 64
    st.d $r9, $t0, 72
    st.d $r10, $t0, 80
    st.d $r11, $t0, 88
    # st.d $r12, $t0, 96
    st.d $r13, $t0, 104
    st.d $r14, $t0, 112
    st.d $r15, $t0, 120
    st.d $r16, $t0, 128
    st.d $r17, $t0, 136
    st.d $r18, $t0, 144
    st.d $r19, $t0, 152
    st.d $r20, $t0, 160
    st.d $r21, $t0, 168
    st.d $r22, $t0, 176
    st.d $r23, $t0, 184
    st.d $r24, $t0, 192
    st.d $r25, $t0, 200
    st.d $r26, $t0, 208
    st.d $r27, $t0, 216
    st.d $r28, $t0, 224
    st.d $r29, $t0, 232
    st.d $r30, $t0, 240
    st.d $r31, $t0, 248

    # Save t0
    ld.d $t1, $tp, 16
    st.d $t1, $t0, 96

    # Save tp
    move $t1, $tp
    csrwr $tp, 0x30
    st.d $tp, $t0, 16
    move $tp, $t1

    # Save PC
    csrrd $t1, 0x06
    st.d $t1, $t0, 0

    # Switch to kernel stack
    ld.d $sp, $tp, 8

    move $fp, $zero
    move $a0, $zero

    bl handle_interrupt


    .globl return_from_kernel
    .type return_from_kernel, @function
return_from_kernel:

    # nested_level counter
    ld.d $t1, $tp, 24
    addi.d $t1, $t1, -1
    st.d $t1, $tp, 24

    # Load current task
    ld.d $t0, $tp, 32

    ld.d $t2, $t0, 264
    li.d $t1, 3 # Priviledge mode
    beq $t2, $zero, 1f 
    move $t1, $zero
1:
    ori $t1, $t1, 0x4 # PIE
    csrwr $t1, 0x01

    # Program counter
    ld.d $t1, $t0, 0
    csrwr $t1, 0x06

    # Restore registers
    ld.d $r31, $t0, 248
    ld.d $r30, $t0, 240
    ld.d $r29, $t0, 232
    ld.d $r28, $t0, 224
    ld.d $r27, $t0, 216
    ld.d $r26, $t0, 208
    ld.d $r25, $t0, 200
    ld.d $r24, $t0, 192
    ld.d $r23, $t0, 184
    ld.d $r22, $t0, 176
    ld.d $r21, $t0, 168
    ld.d $r20, $t0, 160
    ld.d $r19, $t0, 152
    ld.d $r18, $t0, 144
    ld.d $r17, $t0, 136
    ld.d $r16, $t0, 128
    ld.d $r15, $t0, 120
    ld.d $r14, $t0, 112
    ld.d $r13, $t0, 104
    # ld.d $r12, $t0, 96
    ld.d $r11, $t0, 88
    ld.d $r10, $t0, 80
    ld.d $r9, $t0, 72
    ld.d $r8, $t0, 64
    ld.d $r7, $t0, 56
    ld.d $r6, $t0, 48
    ld.d $r5, $t0, 40
    ld.d $r4, $t0, 32
    ld.d $r3, $t0, 24
    ld.d $r2, $t0, 16
    ld.d $r1, $t0, 8

    # t0 has to go last
    ld.d $r12, $t0, 96

    ertn



nested_interrupt:
    addi.d $t0, $t0, 1
    st.d $t0, $tp, 24

    # Align stack as needed
    andi $t0, $sp, 0xf
    bne $t0, $zero, 1f

    # Stack aligned
    addi.d $sp, $sp, -256

    addi.d $t0, $sp, 256
    st.d $t0, $sp, 24

    b 2f
1:
    # Stack misaligned
    addi.d $sp, $sp, -264

    addi.d $r21, $sp, 264
    st.d $r21, $sp, 24
2:

    # Save t0
    ld.d $t0, $tp, 16
    st.d $t0, $sp, 96

    # Save registers
    st.d $r1, $sp, 8
    # st.d $r2, $sp, 16
    # st.d $r3, $sp, 24
    st.d $r4, $sp, 32
    st.d $r5, $sp, 40
    st.d $r6, $sp, 48
    st.d $r7, $sp, 56
    st.d $r8, $sp, 64
    st.d $r9, $sp, 72
    st.d $r10, $sp, 80
    st.d $r11, $sp, 88
    # st.d $r12, $sp, 96
    st.d $r13, $sp, 104
    st.d $r14, $sp, 112
    st.d $r15, $sp, 120
    st.d $r16, $sp, 128
    st.d $r17, $sp, 136
    st.d $r18, $sp, 144
    st.d $r19, $sp, 152
    st.d $r20, $sp, 160
    st.d $zero, $sp, 168
    st.d $r22, $sp, 176
    st.d $r23, $sp, 184
    st.d $r24, $sp, 192
    st.d $r25, $sp, 200
    st.d $r26, $sp, 208
    st.d $r27, $sp, 216
    st.d $r28, $sp, 224
    st.d $r29, $sp, 232
    st.d $r30, $sp, 240
    st.d $r31, $sp, 248

    # $tp...
    move $t0, $tp
    csrwr $tp, 0x30
    st.d $tp, $sp, 16
    move $tp, $t0


    # Save PC
    csrrd $t0, 0x06
    st.d $t0, $sp, 0

    move $s0, $sp
    move $a0, $sp

    move $fp, $zero

    bl kernel_interrupt


    ld.d $t0, $tp, 24
    addi.d $t0, $t0, -1
    st.d $t0, $tp, 24


    # Restore everything
    ld.d $t0, $s0, 0
    csrwr $t0, 0x06

    ld.d $r31, $s0, 248
    ld.d $r30, $s0, 240
    ld.d $r29, $s0, 232
    ld.d $r28, $s0, 224
    ld.d $r27, $s0, 216
    ld.d $r26, $s0, 208
    ld.d $r25, $s0, 200
    ld.d $r24, $s0, 192
    # ld.d $r23, $s0, 184
    ld.d $r22, $s0, 176
    ld.d $r21, $s0, 168
    ld.d $r20, $s0, 160
    ld.d $r19, $s0, 152
    ld.d $r18, $s0, 144
    ld.d $r17, $s0, 136
    ld.d $r16, $s0, 128
    ld.d $r15, $s0, 120
    ld.d $r14, $s0, 112
    ld.d $r13, $s0, 104
    ld.d $r12, $s0, 96
    ld.d $r11, $s0, 88
    ld.d $r10, $s0, 80
    ld.d $r9, $s0, 72
    ld.d $r8, $s0, 64
    ld.d $r7, $s0, 56
    ld.d $r6, $s0, 48
    ld.d $r5, $s0, 40
    ld.d $r4, $s0, 32
    ld.d $r3, $s0, 24
    ld.d $r2, $s0, 16
    ld.d $r1, $s0, 8

    ld.d $r23, $s0, 184

    ertn

    .text
    .p2align 12
    .type bootstrap_isr, @function
    .globl bootstrap_isr
bootstrap_isr:
    addi.d $sp, $sp, -8
    ld.d $t0, $sp, 0

    andi $t0, $sp, 0xf
    beq $r21, $zero, b_misaligned

    # Aligned
    addi.d $sp, $sp, -248
    
    st.d $r1, $sp, 8
    st.d $r2, $sp, 16
    # st.d $r3, $sp, 24
    st.d $r4, $sp, 32
    st.d $r5, $sp, 40
    st.d $r6, $sp, 48
    st.d $r7, $sp, 56
    st.d $r8, $sp, 64
    st.d $r9, $sp, 72
    st.d $r10, $sp, 80
    st.d $r11, $sp, 88
    # st.d $r12, $sp, 96
    st.d $r13, $sp, 104
    st.d $r14, $sp, 112
    st.d $r15, $sp, 120
    st.d $r16, $sp, 128
    st.d $r17, $sp, 136
    st.d $r18, $sp, 144
    st.d $r19, $sp, 152
    st.d $r20, $sp, 160
    st.d $r21, $sp, 168
    st.d $r22, $sp, 176
    st.d $r23, $sp, 184
    st.d $r24, $sp, 192
    st.d $r25, $sp, 200
    st.d $r26, $sp, 208
    st.d $r27, $sp, 216
    st.d $r28, $sp, 224
    st.d $r29, $sp, 232
    st.d $r30, $sp, 240

    # t0 saved initially
    ld.d $t0, $sp, 248
    st.d $t0, $sp, 96

    # Stack
    addi.d $t0, $sp, 256
    st.d $t0, $sp, 24

    st.d $r31, $sp, 248

    # PC
    csrrd $t0, 0x06
    st.d $t0, $sp, 0

    b 1f
b_misaligned:
    addi.d $sp, $sp, -256

    st.d $r1, $sp, 8
    st.d $r2, $sp, 16
    # st.d $r3, $sp, 24
    st.d $r4, $sp, 32
    st.d $r5, $sp, 40
    st.d $r6, $sp, 48
    st.d $r7, $sp, 56
    st.d $r8, $sp, 64
    st.d $r9, $sp, 72
    st.d $r10, $sp, 80
    st.d $r11, $sp, 88
    # st.d $r12, $sp, 96
    st.d $r13, $sp, 104
    st.d $r14, $sp, 112
    st.d $r15, $sp, 120
    st.d $r16, $sp, 128
    st.d $r17, $sp, 136
    st.d $r18, $sp, 144
    st.d $r19, $sp, 152
    st.d $r20, $sp, 160
    st.d $r21, $sp, 168
    st.d $r22, $sp, 176
    st.d $r23, $sp, 184
    st.d $r24, $sp, 192
    st.d $r25, $sp, 200
    st.d $r26, $sp, 208
    st.d $r27, $sp, 216
    st.d $r28, $sp, 224
    st.d $r29, $sp, 232
    st.d $r30, $sp, 240

    # t0 saved initially
    ld.d $t0, $sp, 264
    st.d $t0, $sp, 96

    # Stack
    addi.d $t0, $sp, 264
    st.d $t0, $sp, 24

    st.d $r31, $sp, 248

    # PC
    csrrd $t0, 0x06
    st.d $t0, $sp, 0

1:

    xor $fp, $fp, $fp

    move $a0, $sp

    bl kernel_interrupt

    ld.d $t0, $sp, 0
    csrwr $t0, 0x06

    ld.d $r31, $sp, 248
    ld.d $r30, $sp, 240
    ld.d $r29, $sp, 232
    ld.d $r28, $sp, 224
    ld.d $r27, $sp, 216
    ld.d $r26, $sp, 208
    ld.d $r25, $sp, 200
    ld.d $r24, $sp, 192
    ld.d $r23, $sp, 184
    ld.d $r22, $sp, 176
    ld.d $r21, $sp, 168
    ld.d $r20, $sp, 160
    ld.d $r19, $sp, 152
    ld.d $r18, $sp, 144
    ld.d $r17, $sp, 136
    ld.d $r16, $sp, 128
    ld.d $r15, $sp, 120
    ld.d $r14, $sp, 112
    ld.d $r13, $sp, 104
    ld.d $r12, $sp, 96
    ld.d $r11, $sp, 88
    ld.d $r10, $sp, 80
    ld.d $r9, $sp, 72
    ld.d $r8, $sp, 64
    ld.d $r7, $sp, 56
    ld.d $r6, $sp, 48
    ld.d $r5, $sp, 40
    ld.d $r4, $sp, 32
    ld.d $r2, $sp, 16
    ld.d $r1, $sp, 8
    ld.d $r3, $sp, 24

    ertn