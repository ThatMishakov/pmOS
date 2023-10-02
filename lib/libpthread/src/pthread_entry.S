    .section .text
    .type __pthread_entry_point, @function
    .globl __pthread_entry_point
__pthread_entry_point:
    # Set up stack frame

    movq $0, %rbp
    pushq %rbp
	pushq %rbp

    
    # Move the first argument (the function pointer) into %rax
    movq %rdi, %rax
    # Move the second argument (the argument pointer) into %rdi (first function argument)
    movq %rsi, %rdi

    # Call the function
    callq *%rax


    # Pass the return value of the function to pthread_exit()
    movq %rax, %rdi
    # pthread_exit() outa here!
    # Use jump since pthread_exit() is noreturn
    jmp __pthread_exit




    .type __pthread_exit, @function
    .globl __pthread_exit
__pthread_exit:
    # This function calls appropriate destructors (__cxa_thread_atexit stuff and also atexit() stuff if it's the last thread),
    # destroys the TLS (if the thread is not detached), frees its stack (since kernel doesn't manage it) and exits the task


    # Call __cxa_thread_atexit destructors and stuff
    # This function is defined somewhere in libc
    callq __thread_exit_fire_destructors


    # The TLS struct with explanations is declared in sysroot/usr/include/pmos/tls.h
    # Save the return value to the TLS
    movq %rax, %fs:0x18 # void * return_value
    movq %rax, %r15 # Also pass it to SYSCALL_EXIT; Save it to a protected register so that it can be used after destroying TLS
    # Save the stack data into a protected registers (so that it can be used after destroying TLS)
    movq %fs:0x8, %r14 # void * stack_top
    movq %fs:0x10, %r13 # size_t stack_size


    # Destroy the TLS (the function decides whether to free it or not)
    callq __thread_exit_destroy_tls

    # Free the stack!
    # Since the is no stack, syscalls have to be done manually from here on

    # This is calling release_region() syscall, declared in <pmos/memory.h>
    # TODO: %rax is a convention but it's not used in pmOS
    # Syscall ID
    movq $25, %rdi # SYSCALL_DELETE_REGION
    # uint64_t pid
    movq $0, %rsi # PID_SELF
    # void * region
    movq %r14, %rdx # stack_top
    subq %r13, %rdx # stack_top - stack_size = stack_bottom -> start of the memory region
    # The rest of registers are not used. Zero them out for future compatibility and whatnot
    xorq %r10, %r10 # %r10 is used instead of %rcx because syscall instruction owerwrites it
    xorq %r8, %r8
    xorq %r9, %r9

    # zap the stack!
    syscall
    # It doesn't really matter if it succeeds or not, since the thread is exiting anyway


    # Exit the task
    movq $0, %rdi # SYSCALL_EXIT
    movq %r15, %rsi # return_value
    syscall
    # The task is dead, long live the task!