#include <wctype.h>

int iswalpha(wint_t wc)
{
    return (wc >= L'a' && wc <= L'z') || (wc >= L'A' && wc <= L'Z');
}