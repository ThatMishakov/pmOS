/* ------------------------------------------- MACROS --------------------------------------------- */

    .macro save_scratch
    pushq %rax
    movq %gs:24, %rax

    movq %rdi, 0(%rax)
    movq %rsi, 8(%rax)
    movq %rdx, 16(%rax)
    movq %rcx, 24(%rax)
    movq %r8, 32(%rax)
    movq %r9, 40(%rax)
    popq 48(%rax)
    movq %r10, 56(%rax)
    movq %r11, 64(%rax)
    .endm

    .macro restore_scratch
    movq %gs:24, %rax
    movq 0(%rax), %rdi
    movq 8(%rax), %rsi
    movq 16(%rax), %rdx
    movq 24(%rax), %rcx
    movq 32(%rax), %r8
    movq 40(%rax), %r9
    movq 56(%rax), %r10
    movq 64(%rax), %r11
    movq 48(%rax), %rax
    .endm

    .macro save_context
    movq       %gs:24, %r11
    popq       72(%r11)
    popq       80(%r11)
    popq       88(%r11)
    popq       96(%r11)
    popq       104(%r11)
    movq %rbx, 112(%r11)
    movq %rbp, 120(%r11)
    movq %r12, 128(%r11)
    movq %r13, 136(%r11)
    movq %r14, 144(%r11)
    movq %r15, 152(%r11)
    movl $0xC0000100, %ecx
    rdmsr
    movl %eax, 160(%r11)
    movl %edx, 164(%r11)
    movl $0xC0000102, %ecx
    rdmsr
    movl %eax, 168(%r11)
    movl %edx, 172(%r11)
    movq $0,   176(%r11)
    .endm

    .macro restore_context
    movq       %gs:24, %r11
    pushq      104(%r11)
    pushq      96(%r11)
    pushq      88(%r11)
    pushq      80(%r11)
    pushq      72(%r11)
    movq       112(%r11), %rbx
    movq       120(%r11), %rbp
    movq       128(%r11), %r12
    movq       136(%r11), %r13
    movq       144(%r11), %r14
    movq       152(%r11), %r15
    movl $0xC0000100, %ecx
    movl       160(%r11), %eax
    movl       164(%r11), %edx
    wrmsr
    movl $0xC0000102, %ecx
    movl       168(%r11), %eax
    movl       172(%r11), %edx
    wrmsr
    .endm

    .altmacro
    .macro interrupt_err num
    .type isr\num, @function
isr\num\(): \
    push $\num
    jmp ints_handler
    .endm

    .macro interrupt_noerr num
    .type isr\num, @function
isr\num\():
    push $0
    push $\num
    jmp ints_handler
    .endm

    .macro fill funct, from, to
    .local i
    .set i, \from
    .rept \to-\from+1
    \funct %i
    .set i, i+1
    .endr
    .endm


#define ISR 0x01
#define INTGATE 0x8e
#define TRAPGATE 0xee

    .macro init_intgate isr_n
    leaq isr\isr_n(%rip), %rdi
    leaq (16*\isr_n) + k_idt(%rip), %rsi
    call set_gate_desc_int
    .endm

    .macro init_trapgate isr_n
    leaq isr\isr_n(%rip), %rdi
    leaq (16*\isr_n) + k_idt(%rip), %rsi
    call set_gate_desc_trap
    .endm

/* ------------------------------------------ ISRs -------------------------------------------------- */

    .section .data
    fill interrupt_noerr, 0,7
    interrupt_err         8
    interrupt_noerr       9
    fill interrupt_err,   10,14
    fill interrupt_noerr  15,16
    interrupt_err         17
    fill interrupt_noerr, 18,29
    interrupt_err         30
    interrupt_noerr       31
    fill interrupt_noerr, 32, 251
    fill interrupt_noerr, 253, 255

    .globl isr252 # APIC DUMMY ISR
    .type isr252, @function
isr252:
    movl $0, (0x0b0 + apic_mapped_addr)(%eip)
    iret


/* ------------------------------------------ IDT --------------------------------------------------- */


    .section .data
    .type set_idt_e, @function
set_gate_desc_int:
    movw %di, 0(%rsi)
    movw $0x08, 2(%rsi)
    movw $(ISR | (INTGATE << 8)), 4(%rsi)
    shrq $16, %rdi
    movw %di, 6(%rsi)
    shrq $16, %rdi
    movl %edi, 8(%rsi)
    movl $0, 12(%rsi)
    ret

    .section .data
    .type set_idt_e, @function
set_gate_desc_trap:
    movw %di, 0(%rsi)
    movw $0x08, 2(%rsi)
    movw $(ISR | (TRAPGATE << 8)), 4(%rsi)
    shrq $16, %rdi
    movw %di, 6(%rsi)
    shrq $16, %rdi
    movl %edi, 8(%rsi)
    movl $0, 12(%rsi)
    ret

    .section .bss
    .align 4096
    .globl k_idt
k_idt:
    .space 4096

    .section .data
    .globl fill_idt
    .type fill_idt, @function
fill_idt:
    pushq %rbp
    movq %rsp, %rbp
    init_intgate 0
    init_trapgate 1
    init_intgate 2
    init_trapgate 3
    init_trapgate 4
    fill init_intgate, 5, 201
    init_trapgate 202
    fill init_intgate 203, 255
    //movq 0(%rbp), %rsp
    popq %rbp
    ret



/* ------------------------------------------ FUNCTIONS ---------------------------------------------- */


    .globl loadIDT
    .type loadIDT, @function
loadIDT:
    lidt (%rdi)
    ret

    .globl ints_handler
    .type ints_handler, @function
ints_handler:
    cmpw $0x08, 24(%rsp)
	je 1f
	swapgs
1:
    save_scratch
    movq %gs:24, %rax
    movq $1, 200(%rax)

    cmpq $0xCA, 0(%rsp) 
    jne not_syscall

    addq $16, %rsp
    pushq %rsp
    call syscall_handler
    addq $8, %rsp
    jmp end
not_syscall:
    popq %rdi
    popq %rsi
    leaq (%rsp), %rdx
    call interrupt_handler
end:
    cmpl $0, %gs:40
    je ret_from_interrupt
    cmpl $0, %gs:24
    jne save_cont
    addq $40, %rsp
    jmp task_switch_external
save_cont:
    save_context
    jmp task_switch_external
ret_from_switch_int:
    restore_context
ret_from_interrupt:
    movq %gs:24, %rax
    movq $0, 200(%rax)
    restore_scratch
    cmpw $0x08, 0x8(%rsp)
	je 1f
	swapgs
1:
    iretq

    .globl task_switch_external
    .type task_switch_external, @function
task_switch_external:
    // TODO: INvalidate SSE stuff here

    
    call task_switch

    .globl start_first_process
start_first_process:
    movq %gs:24, %r15
    cmpq $0, 176(%r15)
    je ret_from_switch_int
    // TODO: Return from syscalls
    hlt
