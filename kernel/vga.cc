#include "common/types.h"
#include "paging.hh"
#include "misc.hh"

/*  Some screen stuff. */
/*  The number of columns. */
#define COLUMNS                 80
/*  The number of lines. */
#define LINES                   24
/*  The attribute of an character. */
#define ATTRIBUTE               7
/*  The video memory address. */
#define VIDEO                   0xB8000

/*  Variables. */
/*  Save the X position. */
static int xpos;
/*  Save the Y position. */
static int ypos;
/*  Point to the video memory. */
static volatile unsigned char *video = nullptr;

char screen_init = 0;

/*  Put the character C on the screen. */
void putchar (int c)
{
  // Print to bochs
  printc(c);

  if (video == nullptr) return;

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

void cls (void)
{
  int i;
  
  for (i = 0; i < COLUMNS * LINES * 2; i++)
    *(video + i) = 0;

  xpos = 0;
  ypos = 0;
}

void init_video()
{
    video = (volatile unsigned char*)unoccupied;
    unoccupied = (void*)((uint64_t)unoccupied + 4096);
    Page_Table_Argumments pta;
    pta.writeable = 1;
    pta.user_access = 0;
    map(VIDEO, (uint64_t)video, pta);
    cls();
}