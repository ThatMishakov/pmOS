#include <wchar.h>

int iswspace(wint_t wc)
{
    return wc == L' ' || (wc >= L'\t' && wc <= L'\r');
}