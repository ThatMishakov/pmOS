#include <wctype.h>

int iswdigit(wint_t wc)
{
    return wc >= L'0' && wc <= L'9';
}