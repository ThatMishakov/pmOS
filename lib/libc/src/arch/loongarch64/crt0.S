    .section .text
    .globl _start
_start:
    # Set up frame pointer
    addi.d $sp, $sp, -16
    st.d $zero, $sp, 0 # FP
    st.d $zero, $sp, 8 # RA
    addi.d $fp, $sp, 16

    # Initialize libc
    bl init_std_lib

    # Call main function of the worker
    # This is the first function that runs, and it spawns a thread for the actual main
    bl worker_main

    # This should never be reached
    # UNIMP should always cause an illegal instruction exception
    break 0

    .section .text
    .type __main, @function
    .global __main
    # Linker order weirdness/hack
    # This needs to be fixed, but for now, this works
__main:
    b main

