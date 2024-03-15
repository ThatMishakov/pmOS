    .include "interrupts/asm_macros.S"
    
    .macro save_regs
    # Save %r15 and load current_task pointer into %r15
    movq %r15, %gs:16
    movq %gs:32, %r15

    # Scratch regs
    movq %rdi, 0(%r15)
    movq %rsi, 8(%r15)
    movq %rdx, 16(%r15)
    movq %r10, 24(%r15) # %r10 -> %rcx
    movq %r8, 32(%r15)
    movq %r9, 40(%r15)
    movq %rax, 48(%r15)
    movq %r10, 56(%r15)
    movq %r11, 64(%r15)


    # Preserved regs
    movq %rbx, 112(%r15)
    movq %rbp, 120(%r15)
    movq %r12, 128(%r15)
    movq %r13, 136(%r15)
    movq %r14, 144(%r15)

    movq %gs:16, %rdi
    movq %rdi, 152(%r15)

    # Stack
    movq %rsp, 96(%r15)

    # Compaitability with iret
    movq %rcx, 72(%r15)
    movq %r11, 88(%r15)
    .endm

    .macro restore_regs
    movq       %gs:32, %r15


    movq        0(%r15), %rdi
    movq        8(%r15), %rsi
    movq       16(%r15), %rdx
    movq       32(%r15), %r8
    movq       40(%r15), %r9
    movq       48(%r15), %rax
    movq       56(%r15), %r10

    movq       72(%r15), %rcx
    movq       88(%r15), %r11
    movq       96(%r15), %rsp

    movq      112(%r15), %rbx
    movq      120(%r15), %rbp
    movq      128(%r15), %r12
    movq      136(%r15), %r13
    movq      144(%r15), %r14
    movq      152(%r15), %r15
    .endm



    .macro save_regs_sysenter
    # Save %r15 and load current_task pointer into %r15
    movq %r15, %gs:16
    movq %gs:32, %r15

    # Scratch regs
    movq %rdi, 0(%r15)
    movq %rsi, 8(%r15)
    movq %r11, 16(%r15) # %r11 -> %rdx
    movq %r10, 24(%r15) # %r10 -> %rcx
    movq %r8, 32(%r15)
    movq %r9, 40(%r15)
    movq %rax, 48(%r15)
    movq %r10, 56(%r15)
    movq %r11, 64(%r15)


    # Preserved regs
    movq %rbx, 112(%r15)
    movq %rbp, 120(%r15)
    movq %r12, 128(%r15)
    movq %r13, 136(%r15)
    movq %r14, 144(%r15)

    movq %gs:16, %rdi
    movq %rdi, 152(%r15)

    # Compaitability with iret
    movq %rdx, 72(%r15)
    movq %rcx, 96(%r15)
    pushfq
    popq 88(%r15)
    .endm

    .macro restore_regs_sysenter
    movq       %gs:32, %r15

    movq        0(%r15), %rdi
    movq        8(%r15), %rsi
    movq       32(%r15), %r8
    movq       40(%r15), %r9
    movq       48(%r15), %rax
    movq       56(%r15), %r10
    movq       16(%r15), %r11

    movq       96(%r15), %rcx
    movq       72(%r15), %rdx
    pushq      88(%r15)
    popfq

    movq      112(%r15), %rbx
    movq      120(%r15), %rbp
    movq      128(%r15), %r12
    movq      136(%r15), %r13
    movq      144(%r15), %r14
    movq      152(%r15), %r15
    .endm



    .globl syscall_entry
syscall_entry:
    # SYSCALL will always be from ring 0 to ring 3
    swapgs

    # Save task's registers
    save_regs

    # Entry type
    movq $1,   176(%r15)

    # Switch to kernel's stack
    movq %gs:8, %rsp

    # Stack frame conventions
    movq $0, %rbp

    call syscall_handler

    # Syscall might do a task switch, which might need to be returned to by iret
    # Check that and change return method if that's the case
    movq %gs:32, %r15
    movq 176(%r15), %r15
    jmpq *return_table(,%r15, 8)

    .globl ret_from_syscall
ret_from_syscall:
    
    # Restore registers (and also user's stack)
    restore_regs

    # SYSRET will always be from ring 3 to ring 0
    swapgs

    # Return
    sysretq



    .globl sysenter_entry
sysenter_entry:

    swapgs

    # Save task's registers
    save_regs_sysenter

    # Entry type
    movq $2,   176(%r15)

    call syscall_handler

    # Jmp to the right return method (in case there was a task switch)
    movq %gs:32, %r15
    movq 176(%r15), %r15
    jmpq *return_table(,%r15, 8)


    .globl ret_from_sysenter
ret_from_sysenter:

    # Restore registers
    restore_regs_sysenter

    # SYSEXIT will always be from ring 3 to ring 0
    swapgs

    # Enable interrupts since sysexit doesn't do that
    sti

    # Return
    sysexitq



    .globl syscall_int_entry
syscall_int_entry:

    # It will always be from other rings to ring 0
    swapgs

    # Save registers
    call save_context

    # Entry type
    movq $0,   176(%r11)

    call syscall_handler

    # Jmp to the right return method (in case there was a task switch)
    movq %gs:32, %r15
    movq 176(%r15), %r15
    jmpq *return_table(,%r15, 8)


    .globl ret_repeat_syscall
ret_repeat_syscall:

    # Load current_task into %r15
    movq %gs:32, %r15

    # Restore entry_mode
    movq 200(%r15), %rax
    movq %rax, 176(%r15)
    movq $0, 200(%r15)

    call syscall_handler

    # Jmp to the right return method (in case there was a task switch)
    movq %gs:32, %r15
    movq 176(%r15), %r15
    jmpq *return_table(,%r15, 8)
