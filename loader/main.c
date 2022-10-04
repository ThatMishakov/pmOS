#include <multiboot2.h>
#include <stdint.h>
#include <misc.h>

void print_str(char * str)
{
    while (*str != '\0') { 
        printc(*str);
        ++str;
    }
    return;
}

void main(unsigned long magic, unsigned long addr)
{
    /* Initialize everything */
    print_str("Hello from loader!\n");
    while (1) ;
}