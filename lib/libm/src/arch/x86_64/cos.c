double cos(double x) {
    double result;
    __asm__ (
        "fldl %1;       \n"  // Load x into FPU stack
        "fcos;           \n"  // Compute cosine
        "fstpl %0;       \n"  // Store result in memory
        : "=m" (result)
        : "m" (x)
    );
    return result;
}