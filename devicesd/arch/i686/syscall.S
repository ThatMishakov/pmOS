    # syscall_r syscall_prepare_sleep(uint64_t pid, uint32_t attr, uint32_t value)

    .text
    .globl syscall_prepare_sleep
    .type syscall_prepare_sleep, @function
syscall_prepare_sleep:
    movl %ebx, %gs:ebx@ntpoff
    movl %esi, %gs:esi@ntpoff
    movl %edi, %gs:edi@ntpoff
    movl $ebp, %gs:ebp@ntpoff

    movl (%esp), %eax
    movl 4(%esp), %ecx
    movl %eax, %gs:return_addr@ntpoff
    movl %ecx, %gs:return_ptr@ntpoff

    movl $14, %eax
    movl 8(%esp), %ebx
    movl 12(%esp), %ecx
    movl 16(%esp), %edx
    movl 20(%esp), %ebp

    int $0xf8

    movl %gs:return_ptr@ntpoff, %ecx
    movl %eax, (%ecx)
    movl %edx, 4(%ecx)
    movl %esi, 8(%ecx)
    movl %edi, 12(%ecx)
    movl %ecx, %eax

    movl %gs:ebx@ntpoff, %ebx
    movl %gs:esi@ntpoff, %esi
    movl %gs:edi@ntpoff, %edi
    movl %gs:ebp@ntpoff, %ebp

    addl $8, %esp
    jmpl *%gs:return_addr@ntpoff

    .section .tbss
    .align 4
ebx: .long 0
esi: .long 0
edi: .long 0
ebp: .long 0
return_addr: .long 0
return_ptr: .long 0