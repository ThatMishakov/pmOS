#include <wchar.h>
#include <stdio.h>

float wcstof(const wchar_t*, wchar_t**)
{
    fprintf(stderr, "Error: pmOS libc: wcstof not implemented!\n");
    return 0;
}

double wcstod(const wchar_t*, wchar_t**)
{
    fprintf(stderr, "Error: pmOS libc: wcstod not implemented!\n");
    return 0;
}

long double wcstold(const wchar_t*, wchar_t**)
{
    fprintf(stderr, "Error: pmOS libc: wcstold not implemented!\n");
    return 0;
}

int swprintf(wchar_t*, size_t, const wchar_t*, ...)
{
    fprintf(stderr, "Error: pmOS libc: swprintf not implemented!\n");
    return 0;
}