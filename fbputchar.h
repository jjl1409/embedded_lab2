#ifndef _FBPUTCHAR_H
#define _FBPUTCHAR_H
#include <stdint.h>
#include <stdbool.h>
#define FBOPEN_DEV -1         /* Couldn't open the device */
#define FBOPEN_FSCREENINFO -2 /* Couldn't read the fixed info */
#define FBOPEN_VSCREENINFO -3 /* Couldn't read the variable info */
#define FBOPEN_MMAP -4        /* Couldn't mmap the framebuffer memory */
#define FBOPEN_BPP -5         /* Unexpected bits-per-pixel */
#define MAX_ROWS 24
#define MAX_COLS 64
#define MESSAGE_BOX_ROWS MAX_ROWS - 3
#define MESSAGE_SIZE 128

struct position {
  uint8_t msg_buff_col_indx;
  uint8_t msg_buff_row_indx;
  uint8_t cursor_col_indx;
  uint8_t cursor_row_indx;
  uint8_t msg_buff_indx;
  bool isBackSpacing;
};

extern int fbopen(void);
extern void fbputchar(char, int, int);
extern void fbputs(const char *, int, int);

#endif
