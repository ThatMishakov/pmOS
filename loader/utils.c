#include <utils.h>

char strcmp(char* str1, char* str2)
{
    while (*str1 != '\0') {
        if (*str1 != *str2) return 0;
        ++str1; ++str2;
    }

    if (str2 != 0) return 0;

    return 1;
}

char str_starts_with(char* str1, char* str2)
{
    while (*str2 != '\0') {
        if (*str1 == '\0' || *str1 != *str2) return 0;
        ++str1; ++str2;
    }
    return 1;
}

#define COLUMNS                 80
#define LINES                   24
#define ATTRIBUTE               7
#define VIDEO                   0xB8000

int xpos;
int ypos;
volatile unsigned char *video;

void cls (void)
{
  int i;

  video = (unsigned char *) VIDEO;
  
  for (i = 0; i < COLUMNS * LINES * 2; i++)
    *(video + i) = 0;

  xpos = 0;
  ypos = 0;
}

void
putchar (int c)
{
  static int first = 1;

  if (first) {
    first = 0;
    cls();
  }


  if (c == '\n' || c == '\r')
    {
    newline:
      xpos = 0;
      ypos++;
      if (ypos >= LINES)
        ypos = 0;
      return;
    }

  *(video + (xpos + ypos * COLUMNS) * 2) = c & 0xFF;
  *(video + (xpos + ypos * COLUMNS) * 2 + 1) = ATTRIBUTE;

  xpos++;
  if (xpos >= COLUMNS)
    goto newline;
}
