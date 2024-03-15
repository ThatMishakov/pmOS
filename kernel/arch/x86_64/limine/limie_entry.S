#define STACK_SIZE 4096

    .section .bss
    .align 16
    .globl stack
    .type stack, @object
stack:
    .space STACK_SIZE

    .section .text
    .globl _limine_entry
    .type _limine_entry, @function
_limine_entry:
    # Switch to in-kernel stack, since Limine can't be used because
    # of how how the paging is set up by the kernel
    leaq (stack+STACK_SIZE)(%rip), %rsp

    # Push fake return address and frame pointer
    # this could be helpful to debuggers
    pushq $0
    pushq $0

    call limine_main

    # TODO: This is hacky
    # get_cpu_info()->nested_level = 0;
    movq $0, %gs:24

    jmp start_first_process