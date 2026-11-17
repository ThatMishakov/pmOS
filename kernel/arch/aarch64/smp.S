    .text
    .globl smp_trampoline_start
    .type smp_trampoline_start, %function
smp_trampoline_start:

    mrs x2, currentel

	cmp x2, #8
    b.eq el2

    cmp x2, #4
    b.eq el1

    // EL0 or EL3
halt:
    wfi
    b halt

el2:
    msr cntvoff_el2, xzr
	msr hstr_el2, xzr

    ldr w1, boot_el
    cmp w1, #1
    b.ne enable_vhe

    // Drop to EL1
    // This was taken from Managarm, https://github.com/managarm/managarm
    mov x1, xzr
	orr x1, x1, #(1 << 31)
	orr x1, x1, #(1 << 1)
	msr hcr_el2, x1
	isb

    mov x1, #3 // EL1PCTEN and EL1PCEN
	msr cnthctl_el2, x1

    mov x1, #0x33FF
	msr cptr_el2, x1

    mrs x1, id_aa64pfr0_el1
	ubfx x1, x1, #24, #4
	cbz x1, .L_noGicSre
	mrs x1, icc_sre_el2
	orr x1, x1, #1 // SRE
	orr x1, x1, #8 // Enable
	msr icc_sre_el2, x1
	isb
.L_noGicSre:

    mov x1, #0x3C5
	msr spsr_el2, x1
	adr x1, el1
	msr elr_el2, x1
	mov x3, #0x3C5
	eret

enable_vhe:
    mov x1, xzr
	orr x1, x1, #(1 << 34) // E2H
	orr x1, x1, #(1 << 31) // RW
	orr x1, x1, #(1 << 27) // TGE
	orr x1, x1, #(1 << 1)  // SWIO
	msr hcr_el2, x1
	isb

    // E2H might be cached?
	tlbi alle2
	tlbi alle1
	dsb ish
	isb

el1:
    // Load this before enabling paging, since .data is not mapped here
    ldr x2, trampoline_entry_virt

    ldr x1, mair
    msr mair_el1, x1
    ldr x1, ttbr0
    msr ttbr0_el1, x1
    ldr x1, ttbr1
    msr ttbr1_el1, x1

    ldr x1, tcr
    msr tcr_el1, x1
    isb

    tlbi vmalle1
    dsb nsh
    isb

    ldr x1, sctlr
    msr sctlr_el1, x1
    isb


    br  x2
    .globl smp_trampoline_end
smp_trampoline_end:

smp_trampoline_higher_half:
    msr spsel, #1
    ldr x1, [x0, #8]
    mov  sp, x1

    bl enter_ap

    b return_to_user

    .data
trampoline_entry_virt: .quad smp_trampoline_higher_half