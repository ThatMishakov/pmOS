    .text
    .p2align 12
    .type tlb_refill, @function
    .globl tlb_refill
tlb_refill:
    csrwr $t0, 0x8B   # 00
    csrrd $t0, 0x1B   # 04
    lddir $t0, $t0, 3 # 08
    lddir $t0, $t0, 2 # 0c
    lddir $t0, $t0, 1 # 10
    ldpte   $t0, 0    # 14
    ldpte   $t0, 1    # 18
    tlbfill           # 1c
    csrrd   $t0, 0x8B # 20
    ertn              # 24