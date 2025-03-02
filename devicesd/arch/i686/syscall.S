    # syscall_r syscall_prepare_sleep(uint64_t pid, uint32_t attr, uint32_t value)

    .text
    .globl syscall_prepare_sleep
    .type syscall_prepare_sleep, @function
syscall_prepare_sleep:
    movl %ebx, %gs:ebx@tpoff
    movl %esi, %gs:esi@tpoff
    movl %edi, %gs:edi@tpoff
    movl $ebp, %gs:ebp@tpoff

    movl (%esp), %eax
    movl 4(%esp), %ecx
    movl %eax, %gs:return_addr@tpoff
    movl %ecx, %gs:return_ptr@tpoff

    movl $8, %eax
    movl 8(%esp), %ebx
    movl 12(%esp), %ecx
    movl 16(%esp), %edx
    movl 20(%esp), %ebp

    int $0xf8

    movl %gs:return_ptr@tpoff, %ecx
    movl %eax, (%ecx)
    movl %edx, 4(%ecx)
    movl %esi, 8(%ecx)
    movl %edi, 12(%ecx)
    movl %ecx, %eax

    movl %gs:ebx@tpoff, %ebx
    movl %gs:esi@tpoff, %esi
    movl %gs:edi@tpoff, %edi
    movl %gs:ebp@tpoff, %ebp

    addl $8, %esp
    jmpl *%gs:return_addr@tpoff

    .section .tdata,"awT",@progbits   # TLS data section for initialized thread-local data
    .align 4
ebx: .long
esi: .long
edi: .long
ebp: .long
return_addr: .long
return_ptr: .long