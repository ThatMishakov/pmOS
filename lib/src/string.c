#include "../include/string.h"

size_t strlen(char* str)
{
    size_t size = 0;
    for(;str[size] != '\0'; ++size);

    return size;
}