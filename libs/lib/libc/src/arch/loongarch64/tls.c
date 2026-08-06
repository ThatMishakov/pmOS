void *__get_tp()
{
    void *out;
    __asm__ ("move %0, $tp" : "=r"(out) );
    return out;
}