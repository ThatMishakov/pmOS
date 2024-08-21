#include <wctype.h>

int iswupper(wint_t wc)
{
    return wc >= L'A' && wc <= L'Z';
}