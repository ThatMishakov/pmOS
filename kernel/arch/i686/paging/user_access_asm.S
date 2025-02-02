    .text
    .balign 16, 0x90
    .type fast_copy_to_user, @function
    .globl fast_copy_to_user
fast_copy_to_user:
    pushl %ebp
    movl %esp, %ebp
    pushl %esi
    pushl %edi
    pushl %ebx

    # generic/memory/mem_protection.hh -> Protection::Writeable = 0x02
    pushl $2
    jmp generic_copy


    .type fast_copy_from_user, @function
    .globl fast_copy_from_user
fast_copy_from_user:
    pushl %ebp
    movl %esp, %ebp
    pushl %esi
    pushl %edi
    pushl %ebx

    # User access -> reading
    # generic/memory/mem_protection.hh -> Protection::Readable = 0x01
    pushl $1
generic_copy:

    movl %esp, %gs:28
    leal on_failure, %eax
    movl %eax, %gs:24
    
    # Arguments are passed as follows:
    # 8(%ebp) = ReturnStr<bool> *return_value
    # 12(%ebp) = void *dst
    # 16(%ebp) = const void *src
    # 20(%ebp) = size_t size
continue:
    subl $12, %esp # alignment

    # Basically call memcpy
    pushl 20(%ebp)
    pushl 16(%ebp)
    pushl 12(%ebp)
    
    call memcpy
    addl $28, %esp

    # Return on success
    movl 8(%ebp), %eax
    xorl %edx, %edx
    movl %edx, (%eax)
    movl %edx, 4(%eax)
    addl $1, %edx
    movl %edx, 8(%eax)

    xorl %eax, %eax
    movl %eax, %gs:24

    popl %ebx
    popl %edi
    popl %esi
    popl %ebp
    ret

on_failure:
    movl %eax, %esp
    leal 16(%esp), %ebp

    # Push arguments
    leal 20(%ebp), %eax # size_t &size
    pushl %eax
    leal 16(%ebp), %eax # const void * &src
    pushl %eax
    leal 12(%ebp), %eax # void * &dst
    pushl %eax
    pushl %ecx          # const void *faultung_addr
    pushl -16(%ebp)     # unsigned access
    pushl 8(%ebp)       # ReturnStr<bool> *return_value

    call user_access_page_fault
    
    leal -16(%ebp), %esp

    movl 8(%ebp), %eax
    movl  (%eax), %ecx
    orl  4(%eax), %ecx
    jnz failure

    cmpb $1, 8(%eax)
    je continue
failure:
    xorl %eax, %eax
    movl %eax, %gs:24

    addl $4, %esp
    popl %ebx
    popl %edi
    popl %esi
    popl %ebp
    ret