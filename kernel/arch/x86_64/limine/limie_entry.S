#define STACK_SIZE 4096

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

    # Panic?! Yes!!!
    cli
1:
    hlt
    jmp 1b

    # TODO: Exit to userspace here...