# syscall_r syscall_prepare_sleep(uint64_t pid, uint32_t attr, uint32_t value)
    .text
    .globl syscall_prepare_sleep
    .type syscall_prepare_sleep, @function
syscall_prepare_sleep:
    popq %fs:return_addr@tpoff

    shlq $8, %rdx
    orq $14, %rdx
    xchgq %rdx, %rdi
    xchgq %rsi, %rdx
    
    syscall

    jmpq *%fs:return_addr@tpoff


    .section .tdata,"awT",@progbits   # TLS data section for initialized thread-local data
    .align 8                         # Ensure proper alignment for a 64-bit variable
return_addr:
    .quad 0