void program_syscall()
{
    u64 cpuid = ::cpuid(1);

    write_msr(0xC0000081, ((u64)(R0_CODE_SEGMENT) << 32) | ((u64)(R3_LEGACY_CODE_SEGMENT) << 48)); // STAR (segments for user and kernel code)
    write_msr(0xC0000082, (u64)&syscall_entry); // LSTAR (64 bit entry point)
    write_msr(0xC0000084, (u32)~0x0); // SFMASK (mask for %rflags)

    // Enable SYSCALL/SYSRET in EFER register
    u64 efer = read_msr(0xC0000080);
    write_msr(0xC0000080, efer | (0x01 << 0));

    // This doesn't work on AMD CPUs
    // if (cpuid & (u64(1) << (11 + 32))) {
    //     write_msr(0x174, R0_CODE_SEGMENT); // IA32_SYSENTER_CS
    //     write_msr(0x175, (u64)get_cpu_struct()->kernel_stack_top); // IA32_SYSENTER_ESP
    //     write_msr(0x176, (u64)&sysenter_entry); // IA32_SYSENTER_EIP
    // }
}