#include <wchar.h>
#include <wctype.h>
#include <limits.h>
#include <errno.h>

unsigned long int wcstoul(const wchar_t *nptr, wchar_t **endptr, int base)
{
    const wchar_t *s = nptr;
    int neg       = 0;
    int c;

    do {
        c = *s++;
    } while (iswspace(c));

    if (c == L'-') {
        neg = 1;
        c   = *s++;
    } else if (c == L'+') {
        c = *s++;
    }

    if ((base == 0 || base == 16) && c == L'0' && (*s == L'X' || *s == L'x')) {
        c = s[1];
        s += 2;
        base = 16;
    }

    if (base == 0) {
        base = c == '0' ? 8 : 10;
    }

    long int cutoff = ULONG_MAX / base;
    long int cutlim = ULONG_MAX % base;

    int k           = 0;
    long int result = 0;
    while (c) {
        if (iswdigit(c)) {
            c -= L'0';
        } else if (iswalpha(c)) {
            if (iswupper(c))
                c = c - L'A' + 10;
            else
                c = c - L'a' + 10;
        } else
            break;

        if (c >= base)
            break;

        if (k < 0 || result > cutoff || (result == cutoff && c > cutlim))
            k = -1;
        else {
            k = 1;
            result *= base;
            result += c;
        }
        c = *s++;
    }

    if (k == -1) {
        result = ULONG_MAX;
        errno = ERANGE;
    } else if (neg) {
        result = -result;
    }

    if (endptr != 0) {
        *endptr = (wchar_t *)(k ? s - 1 : nptr);
    }

    return result;
}