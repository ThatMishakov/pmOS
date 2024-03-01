    .globl get_cpu_struct
    .type get_cpu_struct, @function
get_cpu_struct:
    mv a0, tp
    ret

    .globl set_cpu_struct
    .type set_cpu_struct, @function
set_cpu_struct:
    mv tp, a0
    ret