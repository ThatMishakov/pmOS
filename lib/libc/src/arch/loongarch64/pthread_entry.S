    .type __pthread_entry_point, @function
    .globl __pthread_entry_point
__pthread_entry_point:
    move $ra, $zero
    move $fp, $zero

    addi.d $sp, $sp, -16
    st.d $fp, $sp, 0
    st.d $ra, $sp, 8
    addi.d $fp, $sp, 16

    # Start routine
    move $t0, $a0
    # First argument
    move $a0, $a1

    jirl $ra, $t0, 0

    break 0

    .type __pthread_exit, @function
    .globl __pthread_exit
__pthread_exit:
    # The stack has to be aligned to 16 bytes
    addi.d $sp, $sp, -48

    # Do the prelude for debugging and stuff    
    st.d $fp, $sp, 32
    st.d $ra, $sp, 40
    addi.d $fp, $sp, 48

    # Save the saved registers
    # Doesn't really matter as the stack will be freed anyway
    # but can be useful for debugging
    st.d $s0, $sp, 0
    st.d $s1, $sp, 8
    st.d $s2, $sp, 16

    # Save the return value, so it can survive untill syscall_exit
    move $s0, $a0

    # Get thread local data
    bl __get_tls
    move $s2, $a0

    # Save data to TLS and stuff; see x86_64 and sysroot/usr/include/pmos/tls.h
    # for more info
    st.d $s0, $s2, 0x18
    # Load stack pointer into saved registers, to be accessible after
    # destroying TLS
    ld.d $s1, $s2, 0x08 # void * stack_top
    ld.d $s2, $s2, 0x10 # void * stack_size

    # Take care of destructors
    # This should also destroy the TLS
    bl __thread_exit_fire_destructors

    # Free the stack!
    # The stack is allocated as a memory region
    # Syscalls have to be called mamually here since there's no stack
    # for C functions...

    # Syscall ID
    li.d $a0, 26 # SYSCALL_UNMAP_RANGE
    # uint64_t pid (TID in reality, but I digress)
    move $a1, $zero # PID_SELF
    # void * region_start
    sub.w $a2, $s1, $s2 # a2 = stack_top - stack_size
    # size_t region_size
    move $a3, $s2
    # Zero out the rest of registers
    move $a3, $zero
    move $a4, $zero
    move $a5, $zero
    move $a6, $zero

    # Demolish the stack!
    syscall 0

    # Exit the task...
    # Syscall ID
    li.d $a0, 0 # SYSCALL_EXIT
    # Return value
    move $a1, $s0
    syscall 0

    # Crash if the syscall fails (which it shouldn't)
    break 0