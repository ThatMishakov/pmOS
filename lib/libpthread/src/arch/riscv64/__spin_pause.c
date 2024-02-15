void __spin_pause(void)
{
    // PAUSE instruction is called here on x86 machines to hint the processor to
    // wait for a spinlock. I believe I've seen that RISC-V has a similar NOP hint
    // for a CPU, but it doesn't really matter for now
    // TODO: Fix this
}