#include <ctype.h>

int isalnum(int c)
{
    return c >= '0' && c <= '9';
}

int isupper(int c)
{
    return c >= 'A' && c <= 'Z';
}

int islower(int c)
{
    return c >= 'a' && c <= 'z';
}

int isalpha(int c)
{
    return isupper(c) || islower(c);
}

int isspace(int c)
{
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

int tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    } else {
        return c;
    }
}

int isxdigit(int c) {
    return isdigit(c) || (tolower(c) >= 'a' && tolower(c) <= 'f');
}

int isblank(int c) {
    return c == ' ' || c == '\t';
}

int ispunct(int c) {
    return isprint(c) && !isalnum(c) && !isspace(c);
}

int isprint(int c) {
    return c >= ' ' && c <= '~';
}

int iscntrl(int c) {
    return (unsigned char)c < ' ' || c == 127;
}