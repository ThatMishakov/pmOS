.equ STACK_SIZE, 0x4000

.bss
.align 4
.comm stack, STACK_SIZE

.data
.align 3
kernel_smp_at_virt:
    .dword 2f


.text
.globl _limine_entry
.type _limine_entry, @function
_limine_entry:
    # If the kernel was relocatable, gp would need to be set here

    # Switch to our own stack since the one provided by limine will be unusable
    # once the kernel switched to its own page tables
    la.pcrel $sp, stack
    li.d $t0, STACK_SIZE
    add.d $sp, $sp, $t0

    addi.d $sp, $sp, -16
    st.d $zero, $sp, 0
    st.d $zero, $sp, 8
    addi.d $fp, $sp, 16

    bl limine_main

    # Return to userspace
    b return_from_kernel

.equ CSR_CRMD, 0x01
.equ CSR_ECFG, 0x04
.equ CSR_EENTRY, 0xc
.equ CSR_PGDL, 0x19
.equ CSR_PGDH, 0x1a
.equ CSR_PWCL, 0x1c
.equ CSR_PWCH, 0x1d
.equ CSR_STLBPS, 0x1e
.equ CSR_TLBRENTRY, 0x88

.equ CSR_DMW0, 0x180
.equ CSR_DMW1, 0x181
.equ CSR_DMW2, 0x182
.equ CSR_DMW3, 0x183

.equ IOCSR_MAILBOX1, 0x1028


    .globl ap_bringup_trampoline
    .type ap_bringup_trampoline, @function
ap_bringup_trampoline:

    # ----------------------------- Paging ----------------------------
    # Enable direct map
    li.d $t0, 0b010001
    csrwr $t0, CSR_DMW0
    csrwr $zero, CSR_DMW1
    csrwr $zero, CSR_DMW2
    csrwr $zero, CSR_DMW3

    # Load kernel and user (none) page tables...
    la.pcrel  $t1, kernel_ptable_top
    ld.d $t2, $t1, 0
    csrwr $t2, CSR_PGDH
    csrwr $zero, CSR_PGDL

    # Other paging geometry stuff
    la.pcrel $t0, pwcl
    ld.w $t0, $t0, 0
    csrwr $t0, CSR_PWCL
    la.pcrel $t0, pwch
    ld.w $t0, $t0, 0
    csrwr $t0, CSR_PWCH
    la.pcrel $t0, stlbps
    ld.w $t0, $t0, 0
    csrwr $t0, CSR_STLBPS

    # Refill routine...
    la.pcrel $t0, tlb_refill_addr
    ld.d $t0, $t0, 0
    csrwr $t0, CSR_TLBRENTRY

    # Bootstrap isr
    la.pcrel $t0, bootstrap_isr_virt
    ld.d $t0, $t0, 0
    csrwr $t0, CSR_EENTRY
    csrwr $zero, CSR_ECFG

    # Enable paging...
    li.d $t0, 0b1010110000
    csrwr $t0, CSR_CRMD

    # ---------------------- At virt addr ---------------------
    la.pcrel $t0, kernel_smp_at_virt
    ld.d $t0, $t0, 0
    jirl $zero, $t0, 0
2:

    # Load CPU info... It's passed in mailbox 1
    li.d $t0, IOCSR_MAILBOX1
    iocsrrd.d $tp, $t0

    # Stack
    ld.d $sp, $tp, 8

    bl smp_ap_entry

    b return_from_kernel