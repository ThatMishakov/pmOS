    .globl get_cpu_struct
    .type get_cpu_struct, @function
get_cpu_struct:
    mv a0, tp
    ret