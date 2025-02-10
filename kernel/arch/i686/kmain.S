    .bss
bootstrap_stack_bottom:
    .skip 16384
bootstrap_stack_top:

    .text
    .globl _kmain_entry
    .type _kmain_entry, @function
_kmain_entry:
    movl 4(%esp), %eax
    movl 8(%esp), %edx

    movl $bootstrap_stack_top, %esp

    # Align the stack to 16 bytes
    subl $8, %esp

    pushl %edx
    pushl %eax

    call kmain

    jmp return_from_kernel
