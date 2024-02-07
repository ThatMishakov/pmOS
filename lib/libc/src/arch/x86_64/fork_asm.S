    .globl fork
    .type fork, @function
fork:
    # Save the caller's frame pointer.
    pushq   %rbp
    movq    %rsp, %rbp

    # Save the caller's registers for child to restore.
    pushq   %rbx
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15

    # Reserve space for the child information structure.
    subq    $24, %rsp

    # Save the return frame pointer to child_data->return_rbp
    movq    %rbp, %rax
    movq    %rax, (%rsp)

    # Pass the pointer to the 
    movq %rsp,  %rdi

    # Call inner fork function.
    call    __libc_fork_inner

    # Don't restore registers in child.
    movq    %rbp, %rsp
    popq    %rbp
    ret

    .globl __fork_child_entry_point
__fork_child_entry_point:
    # Restore stack pointer
    movq    (%rdi), %rbp
    movq    %rbp, %rsp
    subq    $64, %rsp

    # Pass pointer to child data to function
    movq    %rsp, %rdi

    # Call the child function
    call __libc_fork_child

    # Restore registers
    addq    $24, %rsp

    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbx

    # Return!
    movq    %rbp, %rsp # Not really necessary
    popq    %rbp
    ret