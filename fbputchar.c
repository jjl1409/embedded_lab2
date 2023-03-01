/*
 * fbputchar: Framebuffer character generator
 *
 * Assumes 32bpp
 *
 * References:
 *
 * http://web.njit.edu/all_topics/Prog_Lang_Docs/html/qt/emb-framebuffer-howto.html
 * http://www.diskohq.com/docu/api-reference/fb_8h-source.html
 */

#include "fbputchar.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/fb.h>
#include <linux/string.h>

#define FBDEV "/dev/fb0"
#define FONT_WIDTH 8
#define FONT_HEIGHT 16
#define BITS_PER_PIXEL 32

struct fb_var_screeninfo fb_vinfo;
struct fb_fix_screeninfo fb_finfo;
unsigned char *framebuffer;
static unsigned char font[];

/*
 * Open the framebuffer to prepare it to be written to.  Returns 0 on success
 * or one of the FBOPEN_... return codes if something went wrong.
 */
int fbopen()
{
  int fd = open(FBDEV, O_RDWR); /* Open the device */
  if (fd == -1)
    return FBOPEN_DEV;

  if (ioctl(fd, FBIOGET_FSCREENINFO, &fb_finfo)) /* Get fixed info about fb */
    return FBOPEN_FSCREENINFO;

  if (ioctl(fd, FBIOGET_VSCREENINFO, &fb_vinfo)) /* Get varying info about fb */
    return FBOPEN_VSCREENINFO;

  if (fb_vinfo.bits_per_pixel != 32)
    return FBOPEN_BPP; /* Unexpected */

  framebuffer = mmap(0, fb_finfo.smem_len, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, 0);
  if (framebuffer == (unsigned char *)-1)
    return FBOPEN_MMAP;

  return 0;
}

/*
 * Draw the given character at the given row/column.
 * fbopen() must be called first.
 */
void fbputchar(char c, int row, int col)
{
  int x, y;
  unsigned char pixels, *pixelp = font + FONT_HEIGHT * c;
  unsigned char mask;
  unsigned char *pixel, *left = framebuffer +
                                (row * FONT_HEIGHT * 2 + fb_vinfo.yoffset) * fb_finfo.line_length +
                                (col * FONT_WIDTH * 2 + fb_vinfo.xoffset) * BITS_PER_PIXEL / 8;
  for (y = 0; y < FONT_HEIGHT * 2; y++, left += fb_finfo.line_length)
  {
    pixels = *pixelp;
    pixel = left;
    mask = 0x80;
    for (x = 0; x < FONT_WIDTH; x++)
    {
      if (pixels & mask)
      {
        pixel[0] = 255; /* Red */
        pixel[1] = 255; /* Green */
        pixel[2] = 255; /* Blue */
        pixel[3] = 0;
      }
      else
      {
        pixel[0] = 0;
        pixel[1] = 0;
        pixel[2] = 0;
        pixel[3] = 0;
      }
      pixel += 4;
      if (pixels & mask)
      {
        pixel[0] = 255; /* Red */
        pixel[1] = 255; /* Green */
        pixel[2] = 255; /* Blue */
        pixel[3] = 0;
      }
      else
      {
        pixel[0] = 0;
        pixel[1] = 0;
        pixel[2] = 0;
        pixel[3] = 0;
      }
      pixel += 4;
      mask >>= 1;
    }
    if (y & 0x1)
      pixelp++;
  }
}

void fbline(char c, int row)
{
  int i;
  for (i = 0; i < MAX_COLS; i++)
  {
    fbputchar(c, row, i);
  }
}

void fbscroll(struct position *pos)
{
  unsigned char *textBox = framebuffer + \
                      (TEXT_BOX_START_ROWS * FONT_HEIGHT * 2 + fb_vinfo.yoffset) * fb_finfo.line_length + \
                      (TEXT_BOX_START_COLS * FONT_WIDTH * 2 + fb_vinfo.xoffset) * BITS_PER_PIXEL / 8;
  unsigned char *newTextBox = framebuffer + \
                      ((TEXT_BOX_START_ROWS + 1) * FONT_HEIGHT * 2 + fb_vinfo.yoffset) * fb_finfo.line_length + \
                      (TEXT_BOX_START_COLS * FONT_WIDTH * 2 + fb_vinfo.xoffset) * BITS_PER_PIXEL / 8;
  // Might break with different ROWS, COLS settings. Esp if MESSAGE_BOX_START_COLS > TEXT_BOX_START_BOLS
  ssize_t textBoxSize = ((pos->msg_buff_row_indx - (TEXT_BOX_START_ROWS + 1) - 1) * \
                        FONT_HEIGHT * 2 + fb_vinfo.yoffset) * fb_finfo.line_length + \
                        ((pos->msg_buff_col_indx - TEXT_BOX_START_COLS) * FONT_WIDTH * 2 + fb_vinfo.xoffset) * BITS_PER_PIXEL / 8;

  memmove(textBox, newTextBox, textBoxSize);
  for (int i = pos->msg_buff_col_indx; i < MAX_COLS; i++) {
    fbputchar(' ', pos->msg_buff_row_indx - 1, i);
  }
  pos->msg_buff_row_indx--;
  pos->msg_buff_col_indx = TEXT_BOX_START_COLS;
}

void clearTextBox() {
  for (int i = TEXT_BOX_START_ROWS; i < MESSAGE_BOX_START_ROWS - 1; i++) {
    fbline(' ', i);
  }
}

/*
 * Draw the given string at the given row/column.
 * String must fit on a single line: wrap-around is not handled.
 */
void fbputs(const char *s, int row, int col)
{
  char c;
  while ((c = *s++) != 0)
    fbputchar(c, row, col++);
}

// Handles wrap around
void fbPutString(const char *s, struct position *text_pos) {
  char c;
  bool newLined = false;
  while ((c = *s++) != 0) {
    if (c == 0)
      return;
    else if (c == '\n') {
      text_pos->msg_buff_col_indx = TEXT_BOX_START_COLS;
      text_pos->msg_buff_row_indx++;
      newLined = true;
      continue;
    }
    if ((text_pos->msg_buff_row_indx == MESSAGE_BOX_START_ROWS - 1)) { // So we dont get rid of keys 
      printf("Screen is being cleared!\n");
      //text_pos->msg_buff_col_indx = TEXT_BOX_START_COLS;
      //text_pos->msg_buff_row_indx = TEXT_BOX_START_ROWS;
      //clearTextBox();
      fbscroll(text_pos); // Need to check
    } else if (text_pos->msg_buff_col_indx == MAX_COLS) {
      text_pos->msg_buff_col_indx = TEXT_BOX_START_COLS;
      text_pos->msg_buff_row_indx++;
    }
    fbputchar(c, text_pos->msg_buff_row_indx, text_pos->msg_buff_col_indx);
    text_pos->msg_buff_col_indx++;
  }
  if (!newLined) {
    text_pos->msg_buff_row_indx++;
    text_pos->msg_buff_col_indx = TEXT_BOX_START_COLS;
  }
}


void handleEnterKey(struct position *pos) {
    sendMsg();
    pos->isBackSpacing = false;
    pos->msg_buff_col_indx = MESSAGE_BOX_START_COLS;
    pos->msg_buff_row_indx = MESSAGE_BOX_START_ROWS;
    pos->msg_buff_indx = 0; // Message is sent
    fbline(' ', MAX_ROWS - 3);
    fbline(' ', MAX_ROWS - 2);
}

void handleBackSpace(struct position *pos) {
    printf("%d %d\n", pos->msg_buff_row_indx, pos->msg_buff_col_indx);
    pos->isBackSpacing = true;
    if (pos->msg_buff_col_indx == 0 && pos->msg_buff_row_indx == MESSAGE_BOX_START_ROWS) {
      fbputchar(' ', pos->msg_buff_row_indx, pos->msg_buff_col_indx);
      return;
    } else if (pos->msg_buff_col_indx == 0) {
      fbputchar(' ', pos->msg_buff_row_indx - 1, MAX_COLS - 1);
      pos->msg_buff_col_indx = MAX_COLS - 1;
      pos->msg_buff_row_indx--;
      pos->msg_buff_indx--;
      return;
    }
    fbputchar(' ', pos->msg_buff_row_indx, pos->msg_buff_col_indx - 1);
    pos->msg_buff_indx--; // Might need to remove for Ctrl + Z
    pos->msg_buff_col_indx--;
}

void printChar(struct position *pos, char *msg_buff, char key) {
    if (pos->msg_buff_indx >= MESSAGE_SIZE) {
      return;
    }
    msg_buff[pos->msg_buff_indx] = key;
    fbputchar(key, pos->msg_buff_row_indx, pos->msg_buff_col_indx);
    pos->msg_buff_indx++;
    pos->msg_buff_col_indx++;
    /* if we hit the end of the screen go to the next row and reset colun index*/
    if (pos->msg_buff_col_indx == MAX_COLS)
    {
      pos->msg_buff_col_indx = MESSAGE_BOX_START_COLS;
      pos->msg_buff_row_indx++;
    }
}

/* 8 X 16 console font from /lib/kbd/consolefonts/lat0-16.psfu.gz

od --address-radix=n --width=16 -v -t x1 -j 4 -N 2048 lat0-16.psfu

*/

static unsigned char font[] = {
    0x00,
    0x00,
    0x7e,
    0xc3,
    0x99,
    0x99,
    0xf3,
    0xe7,
    0xe7,
    0xff,
    0xe7,
    0xe7,
    0x7e,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x76,
    0xdc,
    0x00,
    0x76,
    0xdc,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x6e,
    0xf8,
    0xd8,
    0xd8,
    0xdc,
    0xd8,
    0xd8,
    0xd8,
    0xf8,
    0x6e,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x6e,
    0xdb,
    0xdb,
    0xdf,
    0xd8,
    0xdb,
    0x6e,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x38,
    0x7c,
    0xfe,
    0x7c,
    0x38,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x88,
    0x88,
    0xf8,
    0x88,
    0x88,
    0x00,
    0x3e,
    0x08,
    0x08,
    0x08,
    0x08,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xf8,
    0x80,
    0xe0,
    0x80,
    0x80,
    0x00,
    0x3e,
    0x20,
    0x38,
    0x20,
    0x20,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x70,
    0x88,
    0x80,
    0x88,
    0x70,
    0x00,
    0x3c,
    0x22,
    0x3c,
    0x24,
    0x22,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x80,
    0xf8,
    0x00,
    0x3e,
    0x20,
    0x38,
    0x20,
    0x20,
    0x00,
    0x00,
    0x00,
    0x00,
    0x11,
    0x44,
    0x11,
    0x44,
    0x11,
    0x44,
    0x11,
    0x44,
    0x11,
    0x44,
    0x11,
    0x44,
    0x11,
    0x44,
    0x11,
    0x44,
    0x55,
    0xaa,
    0x55,
    0xaa,
    0x55,
    0xaa,
    0x55,
    0xaa,
    0x55,
    0xaa,
    0x55,
    0xaa,
    0x55,
    0xaa,
    0x55,
    0xaa,
    0xdd,
    0x77,
    0xdd,
    0x77,
    0xdd,
    0x77,
    0xdd,
    0x77,
    0xdd,
    0x77,
    0xdd,
    0x77,
    0xdd,
    0x77,
    0xdd,
    0x77,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xf0,
    0xf0,
    0xf0,
    0xf0,
    0xf0,
    0xf0,
    0xf0,
    0xf0,
    0xf0,
    0xf0,
    0xf0,
    0xf0,
    0xf0,
    0xf0,
    0xf0,
    0xf0,
    0x0f,
    0x0f,
    0x0f,
    0x0f,
    0x0f,
    0x0f,
    0x0f,
    0x0f,
    0x0f,
    0x0f,
    0x0f,
    0x0f,
    0x0f,
    0x0f,
    0x0f,
    0x0f,
    0x00,
    0x88,
    0xc8,
    0xa8,
    0x98,
    0x88,
    0x00,
    0x20,
    0x20,
    0x20,
    0x20,
    0x3e,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x88,
    0x88,
    0x50,
    0x50,
    0x20,
    0x00,
    0x3e,
    0x08,
    0x08,
    0x08,
    0x08,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x0e,
    0x38,
    0xe0,
    0x38,
    0x0e,
    0x00,
    0xfe,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xe0,
    0x38,
    0x0e,
    0x38,
    0xe0,
    0x00,
    0xfe,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x06,
    0x0c,
    0xfe,
    0x18,
    0x30,
    0xfe,
    0x60,
    0xc0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x06,
    0x1e,
    0x7e,
    0xfe,
    0x7e,
    0x1e,
    0x06,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xc0,
    0xf0,
    0xfc,
    0xfe,
    0xfc,
    0xf0,
    0xc0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x3c,
    0x7e,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x7e,
    0x3c,
    0x18,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x0c,
    0xfe,
    0x0c,
    0x18,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x30,
    0x60,
    0xfe,
    0x60,
    0x30,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x3c,
    0x7e,
    0x18,
    0x18,
    0x18,
    0x18,
    0x7e,
    0x3c,
    0x18,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x28,
    0x6c,
    0xfe,
    0x6c,
    0x28,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x06,
    0x36,
    0x66,
    0xfe,
    0x60,
    0x30,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x80,
    0xfe,
    0x6e,
    0x6c,
    0x6c,
    0x6c,
    0x6c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x3c,
    0x3c,
    0x3c,
    0x18,
    0x18,
    0x18,
    0x00,
    0x18,
    0x18,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x66,
    0x66,
    0x66,
    0x24,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x6c,
    0x6c,
    0xfe,
    0x6c,
    0x6c,
    0x6c,
    0xfe,
    0x6c,
    0x6c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x10,
    0x7c,
    0xd6,
    0xd0,
    0xd0,
    0x7c,
    0x16,
    0x16,
    0xd6,
    0x7c,
    0x10,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xc2,
    0xc6,
    0x0c,
    0x18,
    0x30,
    0x60,
    0xc6,
    0x86,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x38,
    0x6c,
    0x6c,
    0x38,
    0x76,
    0xdc,
    0xcc,
    0xcc,
    0xcc,
    0x76,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x18,
    0x18,
    0x30,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x0c,
    0x18,
    0x30,
    0x30,
    0x30,
    0x30,
    0x30,
    0x30,
    0x18,
    0x0c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x30,
    0x18,
    0x0c,
    0x0c,
    0x0c,
    0x0c,
    0x0c,
    0x0c,
    0x18,
    0x30,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x66,
    0x3c,
    0xff,
    0x3c,
    0x66,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x18,
    0x7e,
    0x18,
    0x18,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x18,
    0x18,
    0x30,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xfe,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x18,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x06,
    0x0c,
    0x18,
    0x30,
    0x60,
    0xc0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7c,
    0xc6,
    0xce,
    0xce,
    0xd6,
    0xd6,
    0xe6,
    0xe6,
    0xc6,
    0x7c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x38,
    0x78,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x7e,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7c,
    0xc6,
    0x06,
    0x0c,
    0x18,
    0x30,
    0x60,
    0xc0,
    0xc6,
    0xfe,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7c,
    0xc6,
    0x06,
    0x06,
    0x3c,
    0x06,
    0x06,
    0x06,
    0xc6,
    0x7c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x0c,
    0x1c,
    0x3c,
    0x6c,
    0xcc,
    0xfe,
    0x0c,
    0x0c,
    0x0c,
    0x1e,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xfe,
    0xc0,
    0xc0,
    0xc0,
    0xfc,
    0x06,
    0x06,
    0x06,
    0xc6,
    0x7c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x38,
    0x60,
    0xc0,
    0xc0,
    0xfc,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0x7c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xfe,
    0xc6,
    0x06,
    0x06,
    0x0c,
    0x18,
    0x30,
    0x30,
    0x30,
    0x30,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7c,
    0xc6,
    0xc6,
    0xc6,
    0x7c,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0x7c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7c,
    0xc6,
    0xc6,
    0xc6,
    0x7e,
    0x06,
    0x06,
    0x06,
    0x0c,
    0x78,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x18,
    0x00,
    0x00,
    0x00,
    0x18,
    0x18,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x18,
    0x00,
    0x00,
    0x00,
    0x18,
    0x18,
    0x30,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x06,
    0x0c,
    0x18,
    0x30,
    0x60,
    0x30,
    0x18,
    0x0c,
    0x06,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xfe,
    0x00,
    0x00,
    0xfe,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x60,
    0x30,
    0x18,
    0x0c,
    0x06,
    0x0c,
    0x18,
    0x30,
    0x60,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7c,
    0xc6,
    0xc6,
    0x0c,
    0x18,
    0x18,
    0x18,
    0x00,
    0x18,
    0x18,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7c,
    0xc6,
    0xc6,
    0xc6,
    0xde,
    0xde,
    0xde,
    0xdc,
    0xc0,
    0x7c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x38,
    0x6c,
    0xc6,
    0xc6,
    0xfe,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xfc,
    0x66,
    0x66,
    0x66,
    0x7c,
    0x66,
    0x66,
    0x66,
    0x66,
    0xfc,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x3c,
    0x66,
    0xc2,
    0xc0,
    0xc0,
    0xc0,
    0xc0,
    0xc2,
    0x66,
    0x3c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xf8,
    0x6c,
    0x66,
    0x66,
    0x66,
    0x66,
    0x66,
    0x66,
    0x6c,
    0xf8,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xfe,
    0x66,
    0x62,
    0x68,
    0x78,
    0x68,
    0x60,
    0x62,
    0x66,
    0xfe,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xfe,
    0x66,
    0x62,
    0x68,
    0x78,
    0x68,
    0x60,
    0x60,
    0x60,
    0xf0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x3c,
    0x66,
    0xc2,
    0xc0,
    0xc0,
    0xde,
    0xc6,
    0xc6,
    0x66,
    0x3a,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xfe,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x3c,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x3c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x1e,
    0x0c,
    0x0c,
    0x0c,
    0x0c,
    0x0c,
    0xcc,
    0xcc,
    0xcc,
    0x78,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xe6,
    0x66,
    0x66,
    0x6c,
    0x78,
    0x78,
    0x6c,
    0x66,
    0x66,
    0xe6,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xf0,
    0x60,
    0x60,
    0x60,
    0x60,
    0x60,
    0x60,
    0x62,
    0x66,
    0xfe,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xc6,
    0xee,
    0xfe,
    0xfe,
    0xd6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xc6,
    0xe6,
    0xf6,
    0xfe,
    0xde,
    0xce,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7c,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0x7c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xfc,
    0x66,
    0x66,
    0x66,
    0x7c,
    0x60,
    0x60,
    0x60,
    0x60,
    0xf0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7c,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xd6,
    0xde,
    0x7c,
    0x0c,
    0x0e,
    0x00,
    0x00,
    0x00,
    0x00,
    0xfc,
    0x66,
    0x66,
    0x66,
    0x7c,
    0x6c,
    0x66,
    0x66,
    0x66,
    0xe6,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7c,
    0xc6,
    0xc6,
    0x60,
    0x38,
    0x0c,
    0x06,
    0xc6,
    0xc6,
    0x7c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7e,
    0x7e,
    0x5a,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x3c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0x7c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0x6c,
    0x38,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xd6,
    0xd6,
    0xd6,
    0xfe,
    0xee,
    0x6c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xc6,
    0xc6,
    0x6c,
    0x7c,
    0x38,
    0x38,
    0x7c,
    0x6c,
    0xc6,
    0xc6,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x66,
    0x66,
    0x66,
    0x66,
    0x3c,
    0x18,
    0x18,
    0x18,
    0x18,
    0x3c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xfe,
    0xc6,
    0x86,
    0x0c,
    0x18,
    0x30,
    0x60,
    0xc2,
    0xc6,
    0xfe,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x3c,
    0x30,
    0x30,
    0x30,
    0x30,
    0x30,
    0x30,
    0x30,
    0x30,
    0x3c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xc0,
    0x60,
    0x30,
    0x18,
    0x0c,
    0x06,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x3c,
    0x0c,
    0x0c,
    0x0c,
    0x0c,
    0x0c,
    0x0c,
    0x0c,
    0x0c,
    0x3c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x38,
    0x6c,
    0xc6,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xff,
    0x00,
    0x00,
    0x30,
    0x30,
    0x30,
    0x18,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x78,
    0x0c,
    0x7c,
    0xcc,
    0xcc,
    0xcc,
    0x76,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xe0,
    0x60,
    0x60,
    0x78,
    0x6c,
    0x66,
    0x66,
    0x66,
    0x66,
    0x7c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7c,
    0xc6,
    0xc0,
    0xc0,
    0xc0,
    0xc6,
    0x7c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x1c,
    0x0c,
    0x0c,
    0x3c,
    0x6c,
    0xcc,
    0xcc,
    0xcc,
    0xcc,
    0x76,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7c,
    0xc6,
    0xfe,
    0xc0,
    0xc0,
    0xc6,
    0x7c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x38,
    0x6c,
    0x64,
    0x60,
    0xf0,
    0x60,
    0x60,
    0x60,
    0x60,
    0xf0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x76,
    0xcc,
    0xcc,
    0xcc,
    0xcc,
    0xcc,
    0x7c,
    0x0c,
    0xcc,
    0x78,
    0x00,
    0x00,
    0x00,
    0xe0,
    0x60,
    0x60,
    0x6c,
    0x76,
    0x66,
    0x66,
    0x66,
    0x66,
    0xe6,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x18,
    0x00,
    0x38,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x3c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x06,
    0x06,
    0x00,
    0x0e,
    0x06,
    0x06,
    0x06,
    0x06,
    0x06,
    0x06,
    0x66,
    0x66,
    0x3c,
    0x00,
    0x00,
    0x00,
    0xe0,
    0x60,
    0x60,
    0x66,
    0x6c,
    0x78,
    0x78,
    0x6c,
    0x66,
    0xe6,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x70,
    0x30,
    0x30,
    0x30,
    0x30,
    0x30,
    0x30,
    0x30,
    0x34,
    0x18,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xec,
    0xfe,
    0xd6,
    0xd6,
    0xd6,
    0xd6,
    0xc6,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xdc,
    0x66,
    0x66,
    0x66,
    0x66,
    0x66,
    0x66,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7c,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0x7c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xdc,
    0x66,
    0x66,
    0x66,
    0x66,
    0x66,
    0x7c,
    0x60,
    0x60,
    0xf0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x76,
    0xcc,
    0xcc,
    0xcc,
    0xcc,
    0xcc,
    0x7c,
    0x0c,
    0x0c,
    0x1e,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xdc,
    0x76,
    0x66,
    0x60,
    0x60,
    0x60,
    0xf0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7c,
    0xc6,
    0x60,
    0x38,
    0x0c,
    0xc6,
    0x7c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x30,
    0x30,
    0xfc,
    0x30,
    0x30,
    0x30,
    0x30,
    0x36,
    0x1c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xcc,
    0xcc,
    0xcc,
    0xcc,
    0xcc,
    0xcc,
    0x76,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x66,
    0x66,
    0x66,
    0x66,
    0x66,
    0x3c,
    0x18,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xc6,
    0xc6,
    0xd6,
    0xd6,
    0xd6,
    0xfe,
    0x6c,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xc6,
    0x6c,
    0x38,
    0x38,
    0x38,
    0x6c,
    0xc6,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0xc6,
    0x7e,
    0x06,
    0x0c,
    0xf8,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xfe,
    0xcc,
    0x18,
    0x30,
    0x60,
    0xc6,
    0xfe,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x0e,
    0x18,
    0x18,
    0x18,
    0x70,
    0x18,
    0x18,
    0x18,
    0x18,
    0x0e,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x18,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x70,
    0x18,
    0x18,
    0x18,
    0x0e,
    0x18,
    0x18,
    0x18,
    0x18,
    0x70,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x76,
    0xdc,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x66,
    0x00,
    0x66,
    0x66,
    0x66,
    0x66,
    0x3c,
    0x18,
    0x18,
    0x18,
    0x3c,
    0x00,
    0x00,
    0x00,
    0x00,
};
