    .text
    .p2align 12
    .type tlb_refill, @function
    .globl tlb_refill
tlb_refill:
    csrwr $t0, 0x8B
    csrrd $t0, 0x1B
    lddir $t0, $t0, 3
    lddir $t0, $t0, 2
    lddir $t0, $t0, 1
    ldpte   $t0, 0
    ldpte   $t0, 1
    tlbfill
    csrrd   $t0, 0x8B
    ertn