#define ASM_FILE        1
#include <multiboot2.h>
#include <asm.h>

#define GRUB_MULTIBOOT_ARCHITECTURE_I386 0
#define STACK_SIZE 0x4000

/* ------------------------------------------------------- MULTIBOOT2 HEADER ---------------------------------------------------- */
    .code32
    .section .multiboot2
    /*  Align 64 bits boundary. */
    .align  8    
    /*  Multiboot header. */
multiboot_header:
    /*  magic */
    .long   MULTIBOOT2_HEADER_MAGIC
    /*  ISA: i386 */
    .long   GRUB_MULTIBOOT_ARCHITECTURE_I386
    /*  Header length. */
    .long   multiboot_header_end - multiboot_header
    /*  checksum */
    .long   -(MULTIBOOT2_HEADER_MAGIC + GRUB_MULTIBOOT_ARCHITECTURE_I386 + (multiboot_header_end - multiboot_header))
/* Align modules to pages */
module_alligment_tag_start:
    .short  MULTIBOOT_HEADER_TAG_MODULE_ALIGN
    .short  0
    .long   module_alligment_tag_end - module_alligment_tag_start
module_alligment_tag_end: 

/* Set entry point */
entry_address_tag_start:
    .short  MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS
    .short  0
    .long   entry_address_tag_end - entry_address_tag_start
    .quad   _multiboot_start // Half-broken!
entry_address_tag_end:

    /* Terminate multiboot2 header */
    .short MULTIBOOT_HEADER_TAG_END
    .short 0
    .long 8   
multiboot_header_end:

/* ------------------------------------------------------- BASIC INIT ---------------------------------------------------- */
    .section .data
    .globl g_pml4
g_pml4: .quad 0


    .code32
    .text
    .globl  _multiboot_start
    .type _multiboot_start, @function
_multiboot_start:
    /*  Initialize the stack pointer. */
    movl (%ebx), %edx
    movl %ebx, %ecx

    movl %ebx, %esp
    addl $4, %esp
    call rel_addr
rel_addr:
    popl %ebx
    movl %edx, (%ecx)

    #define REL(X) X - rel_addr(%ebx)

    leal    stack + STACK_SIZE - rel_addr(%ebx), %esp

    /*  Reset EFLAGS. */
    pushl   $0
    popf

    /* Save multiboot structures */
    movl %ecx, multiboot_info_str - rel_addr(%ebx)
    movl %eax, multiboot_magic - rel_addr(%ebx)

    /* Load GDT */
    pushl   $(temp_gdt_end - temp_gdt - 1) /* size */

    pushl   $temp_gdt - rel_addr /* base (relative) */
    addl    %ebx, (%esp)

    call    loadGDT_32
    addl    $8, %esp

    /* Init misc stuff */
    call init_free

    /* Get pml4 */
    call get_free_page
    movl %eax, REL(g_pml4)

    pushl REL(g_pml4)
    call clear_page_32
    addl $4, %esp

    /* Init NX */
    pushl %ebx
    movl $0x80000001, %eax
    cpuid
    popl %ebx
    shrl $20, %edx
    andl $1, %edx
    movb %dl, REL(nx_enabled)
    jz nx_not_supported

    /* Enable NX */
    movl $0xc0000080, %ecx
    rdmsr
    orl $(1 << 11), %eax
    wrmsr

nx_not_supported:
    /* Init PML4 */

    /* Point last entry to self */
    movl REL(g_pml4), %ecx
    movl %ecx, %eax
    orl $0x000000403, %eax /* P W*/
    movl %eax, 2048(%ecx)

    /* *casually* map pages */
    leal REL(_exec_start), %esi
    leal REL(_map_p_end ), %edi
mem_loop_start:

    pushl %esi
    call map_page_self
    addl $4, %esp

    addl $4096, %esi
    cmpl %edi, %esi
    jl mem_loop_start


    /* Enable PAE and page size extension */
    movl %cr4, %eax
    orl $((1 << 5) | (1 << 4)), %eax
    movl %eax, %cr4

    /* Enable long mode */
    movl $0xc0000080, %ecx
    rdmsr
    orl $(1 << 8), %eax
    wrmsr

    /* Load PML4 */
    movl REL(g_pml4), %eax
    movl %eax, %cr3

    /* Enable paging */
    movl %cr0, %eax
    orl $((1 << 31) | (1 << 0)), %eax
    movl %eax, %cr0

    /* Load 64 bit segment registers */
    movw $(temp_gdt_64bit_data - temp_gdt), %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    /* Far return to main */
    pushl $(temp_gdt_64bit_code - temp_gdt)
    pushl $(main - rel_addr)
    addl %ebx, (%esp)
    lretl


    /*  Our stack area. */
    .section bss
    .align 4096
    .comm   stack, STACK_SIZE

// ------------------------------------------------------- PAGING ------------------------------------------------------- */

.section .text
    .type init_free, @function
init_free:
    leal _map_p_end - rel_addr(%ebx), %eax
    movl %eax, after_exec_free_avail - rel_addr(%ebx)
    ret

    .type get_free_page, @function
get_free_page:
    movl REL(after_exec_free_avail), %eax
    addl $4096, REL(after_exec_free_avail)
    ret

    .type map_page_self, @function
map_page_self:
    pushl %ebp
    movl %esp, %ebp
    pushl %esi
    pushl %edi

    // Check if PDPT is present
    /*
    movl 8(%ebp), %esi
    shrl $(12 + 9 + 9 + 9), %esi
    andl $0777, %esi
    */
    movl $0, %esi // 0 for 32 bit addresses

    movl REL(g_pml4), %edi
    movl (%edi, %esi, 8), %eax
    btl $0, %eax
    jc pml4e_present

    // Not present, spawn it
    call get_free_page
    pushl %eax
    call clear_page_32    
    popl %eax
    orl $( 1 << 0 | 1 << 1 | 1 << 2), %eax
    movl %eax, (%edi, %esi, 8)

pml4e_present:
    andl $~0777, %eax // %eax now has address of PDPT

    // Check if PDPT is present
    movl %eax, %edi
    movl 8(%ebp), %esi
    shrl $(12 + 9 + 9), %esi
    andl $0777, %esi
    movl (%edi, %esi, 8), %eax
    btl $0, %eax
    jc pdpte_present

    call get_free_page
    pushl %eax
    call clear_page_32
    popl %eax
    orl $( 1 << 0 | 1 << 1 | 1 << 2), %eax
    movl %eax, (%edi, %esi, 8)

pdpte_present:
    andl $~0777, %eax // %eax now has address of PD

    // Check if PD entry is present
    movl %eax, %edi
    movl 8(%ebp), %esi
    shrl $(12 + 9), %esi
    andl $0777, %esi
    movl (%edi, %esi, 8), %eax
    btl $0, %eax
    jc pde_present

    call get_free_page
    pushl %eax
    call clear_page_32
    popl %eax
    orl $( 1 << 0 | 1 << 1 | 1 << 2), %eax
    movl %eax, (%edi, %esi, 8)

pde_present:
    andl $~0777, %eax // %eax now has address of PT
    movl %eax, %edi
    movl 8(%ebp), %esi
    shrl $(12), %esi
    andl $0777, %esi
    movl 8(%ebp), %eax
    orl $( 1 << 0 | 1 << 1 | 1 << 2), %eax
    movl %eax, (%edi, %esi, 8)

    popl %edi
    popl %esi
    popl %ebp
    ret

/* -------------------------------------------------------- MISC -------------------------------------------------------- */

    .type clear_page_32, @function
clear_page_32:
    movl $1024, %ecx
    movl 4(%esp), %edx /* Address */
.start_c:
    movl $0, -4(%edx,%ecx, 4);
    loop .start_c
    ret


/* ------------------------------------------------------- GDT --------------------------------------------------------- */
    .data
GDT_DW:
    .short 0
    .short 0
    .short 0

    .text
    .code32
.text
    .globl loadGDT_32
    .type loadGDT_32, @function
loadGDT_32:
    movw   8(%esp), %ax
    movw   %ax, GDT_DW - rel_addr(%ebx)
    movw   4(%esp), %ax
    movw   %ax, GDT_DW+2 - rel_addr(%ebx)
    movw   6(%esp), %ax
    movw   %ax, GDT_DW+4 - rel_addr(%ebx)
    lgdt   GDT_DW - rel_addr(%ebx)
    pushl $0x08
    pushl $(reload_CS - rel_addr)
    addl %ebx, (%esp)
    lretl
reload_CS:
    movw $0x10, %ax
    movw %ax, %ss
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    ret

    .data
    .align  4096
temp_gdt:
    .quad 0 /* Empty entry */

temp_gdt_32bit_code:
    .short 0xffff /* 32 bit code segment */
    .short 0x0000
    .byte 0x00
    .byte 0x9a
    .byte 0xcf
    .byte 0x00

temp_gdt_32bit_data:
    .short 0xffff /* 32 bit data segment */
    .short 0x0000
    .byte 0x00
    .byte 0x92
    .byte 0xcf
    .byte 0x00

temp_gdt_64bit_code:
    .short 0xffff /* 64 bit code segment */
    .short 0x0000
    .byte 0x00
    .byte 0x9a
    .byte 0xaf
    .byte 0x00

temp_gdt_64bit_data:
    .short 0xFFFF /* 64 bit data segment */
    .short 0x0000
    .byte 0x00
    .byte 0x92 /* ACESS BYTE */
    .byte 0xcf
    .byte 0x00
temp_gdt_end:


    /* ------------------------------------------------------ CPU BOOTSTRAP ---------------------------------------- */
        .code16
    .section .cpuinit
    .globl init_cpucpuinit
    .type init_cpu, @function
init_cpu:
    # Disable interrupts
    # xchgw %bx, %bx
    cli

    # Enable A20
    inb $0x92, %al
    orb $2, %al
    outb %al, $0x92

    # Load GDT
    lgdt GDT_DW

    # Enter protected mode
    movl %cr0, %eax
    orb 1, %al
    movl %eax, %cr0

    jmp $(temp_gdt_32bit_code - temp_gdt),$protected_mode

    .code32
protected_mode:
    movw $(temp_gdt_32bit_data - temp_gdt), %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs

    cmpb $0, nx_enabled
    jz nx_not_supported_bootstrap

    /* Enable NX */
    movl $0xc0000080, %ecx
    rdmsr
    orl $(1 << 11), %eax
    wrmsr

nx_not_supported_bootstrap:
    /* Enable PAE and page size extension */
    movl %cr4, %eax
    orl $((1 << 5) | (1 << 4)), %eax
    movl %eax, %cr4

    /* Enable long mode */
    movl $0xc0000080, %ecx
    rdmsr
    orl $(1 << 8), %eax
    wrmsr
    
    /* Load PML4 */
    movl g_pml4, %eax
    movl %eax, %cr3

    /* Enable paging */
    movl %cr0, %eax
    orl $((1 << 31) | (1 << 0)), %eax
    movl %eax, %cr0

    /* Load 64 bit segment registers */
    movw $(temp_gdt_64bit_data - temp_gdt), %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    jmp $(temp_gdt_64bit_code - temp_gdt), $long_mode
    .code64
long_mode:
    jmp *kernel_cpu_init(%rip)
