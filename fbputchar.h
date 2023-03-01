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
#define MESSAGE_BOX_START_ROWS MAX_ROWS - 3
#define MESSAGE_BOX_START_COLS 0
#define MESSAGE_SIZE 128
#define TEXT_BOX_START_ROWS 8
#define TEXT_BOX_START_COLS 0
#define TAB_SPACING 4
#define RESET_SPECIAL_KEYS(X) {\
            X.caps_lock = false;\
            X.shift_pressed = false;\
            X.backspace_pressed = false;\
            X.escape_pressed = false;\
            RESET_ARROW_KEYS(X);
        }
#define RESET_ARROW_KEYS(X) {\
            X.down_arrow = false;\
            X.up_arrow = false;\
            X.right_arrow = false;\
            X.left_arrow = false;\
}

struct position {
  uint8_t msg_buff_col_indx;
  uint8_t msg_buff_row_indx;
  uint8_t cursor_col_indx;
  uint8_t cursor_row_indx;
  uint8_t msg_buff_indx;
};

struct special_keys {
  bool caps_lock;
  bool left_arrow;
  bool up_arrow;
  bool right_arrow;
  bool down_arrow;
  bool shift_pressed;
  bool backspace_pressed;
  bool escape_pressed;
};

extern int fbopen(void);
extern void fbputchar(char, int, int);
extern void fbputs(const char *, int, int);
struct special_keys s_keys;

#endif
