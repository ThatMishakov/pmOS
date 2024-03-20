    .section .text
    .type __pthread_entry_point, @function
    .globl __pthread_entry_point
    # C function declaration:
    # void __pthread_entry_point(void *(*start_routine) (void *),
    #                            void * arg) __attribute__((noreturn));
__pthread_entry_point:
    # Set up stack frame
    mv ra, zero
    mv fp, zero

    # Function prelude
    addi sp, sp, -16
    sd fp, 0(sp)
    sd ra, 8(sp)
    addi fp, sp, 16

    # Set global pointer
    # https://five-embeddev.com/quickref/global_pointer.html
    .option push
	.option norelax
	la gp, __global_pointer$
	.option pop

    # Move the arguments in the right registers
    # Start routine
    mv t0, a0
    # Second argument of this function -> first argument of start_routine
    mv a0, a1

    # Call the start_routine
    jalr ra, t0, 0

    # Exit the thread
    # Return value is also the first argument
    call __pthread_exit

    # Should never reach here
    unimp




    .type __pthread_exit, @function
    .globl __pthread_exit
__pthread_exit:
    # The stack has to be aligned to 16 bytes
    addi sp, sp, -48

    # Do the prelude for debugging and stuff
    sd fp, 32(sp)
    sd ra, 40(sp)
    addi fp, sp, 48

    # Save the saved registers
    # Doesn't really matter as the stack will be freed anyway
    # but can be useful for debugging
    sd s1, 0(sp)
    sd s2, 8(sp)
    sd s3, 16(sp)

    # Save the return value, so it can survive untill syscall_exit
    mv s1, a0

    # Get thread local data
    call __get_tls
    mv s3, a0

    # Take care of destructors
    call __thread_exit_fire_destructors

    # Save data to TLS and stuff; see x86_64 and sysroot/usr/include/pmos/tls.h
    # for more info
    sd s1, 0x18(s3) # void * return_value
    # Load stack pointer into saved registers, to be accessible after
    # destroying TLS
    ld s2, 0x08(s3) # void * stack_top
    ld t0, 0x10(s3) # void * stack_size

    # Destroy the TLS (the function decides whether to free it or not)
    call __thread_exit_destroy_tls

    # Free the stack!
    # The stack is allocated as a memory region
    # Syscalls have to be called mamually here since there's no stack
    # for C functions...

    # Syscall ID
    li a0, 25 # SYSCALL_DELETE_REGION
    # uint64_t pid (TID in reality, but I digress)
    mv a1, zero # PID_SELF
    # void * region_start
    sub a2, s2, t0 # a2 = stack_top - stack_size
    # Zero out the rest of registers
    mv a3, zero
    mv a4, zero
    mv a5, zero
    mv a6, zero

    # Demolish the stack!
    ecall


    # Exit the task...
    # Syscall ID
    li a0, 0 # SYSCALL_EXIT
    # Return value
    mv a1, s1
    ecall

    # Crash if the syscall fails (which it shouldn't)
    unimp

