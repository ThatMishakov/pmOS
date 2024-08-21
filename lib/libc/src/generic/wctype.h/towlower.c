#include <wctype.h>

wint_t towlower(wint_t wc)
{
    if (iswupper(wc))
        return wc + 32;
    return wc;
}