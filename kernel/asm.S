    .globl printc
    .type printc, @function
printc:
    movb %dil, %al
    outb $0xe9
    ret

    .globl tlb_flush
    .type tlb_flush, @function
tlb_flush:
    movq %cr3, %rax
    movq %rax, %cr3
    ret

    .globl page_clear
    .type page_clear, @function
page_clear:
    movq $512, %rcx
.start_c:
    movq $0, -8(%rdi, %rcx, 8);
    loop .start_c
    retq

   .globl memset
    .type memset, @function
memset:
    movq %rsi, %rcx
.start:
    movq $0, -8(%rdi, %rcx, 8);
    loop .start
    retq

    .globl loadTSS
    .type loadTSS, @function
loadTSS:
    movq %rdi, %rax
    ltr %ax
    ret

    .globl getCR3
    .type getCR3, @function
getCR3:
    movq %cr3, %rax
    ret

    .globl setCR3
    .type setCR3, @function
setCR3:
    movq %rdi, %cr3
    ret

    .globl getCR2
    .type getCR2, @function
getCR2:
    movq %cr2, %rax
    ret

    .globl set_segment_regs
    .type set_segment_regs, @function
set_segment_regs:
    movq %rdi, %rax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    ret

    .globl invlpg
    .type invlpg, @function
invlpg:
    invlpg (%rdi)
    ret

    .globl read_msr
    .type read_msr, @function
read_msr:
    movl %edi, %ecx
    rdmsr
    rorq $32, %rax
    movl %edx, %eax
    rorq $32, %rax
    ret 

    .globl write_msr
    .type write_msr, @function
write_msr:
    movl %edi, %ecx
    movl %esi, %eax
    rorq $32, %rsi
    movl %esi, %edx
    wrmsr
    ret

    .globl get_cpu_struct
    .type get_cpu_struct, @function
get_cpu_struct:
    movq %gs:0, %rax
    ret
