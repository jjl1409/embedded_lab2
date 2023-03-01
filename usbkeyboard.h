#ifndef _USBKEYBOARD_H
#define _USBKEYBOARD_H

#include <libusb-1.0/libusb.h>
#include <stdbool.h>
#include "fbputchar.h"
//#include "/opt/homebrew/Cellar/libusb/1.0.26/include/libusb-1.0/libusb.h"
#define USB_HID_KEYBOARD_PROTOCOL 1
#define MAX_KEYS_PRESSED 6

/* Modifier bits */
#define USB_LCTRL  (1 << 0)
#define USB_LSHIFT (1 << 1)
#define USB_LALT   (1 << 2)
#define USB_LGUI   (1 << 3)
#define USB_RCTRL  (1 << 4)
#define USB_RSHIFT (1 << 5)
#define USB_RALT   (1 << 6) 
#define USB_RGUI   (1 << 7)
/* Fun stuff to make program work */
#define USB_SHIFT_PRESSED(X) (( (X & (USB_LSHIFT | USB_RSHIFT)) > 0) || s_keys.caps_lock)
#define USB_CAPS_LOCK_PRESSED(X) ((X[0] == 0x39) || (X[1] == 0x39) || (X[2] == 0x39) || (X[3] == 0x39) || (X[4] == 0x39) || (X[5] == 0x39))
#define USB_NOTHING_PRESSED(X) ((!X[0]) && (!X[1]) && (!X[2]) && (!X[3]) && (!X[4]) && (!X[5]))
#define USB_ARROW_KEYS_PRESSED(X) \{USB_RIGHT_ARROW_KEY_PRESSED(X) \
                                  || USB_LEFT_ARROW_KEY_PRESSED(X) \
                                  || USB_DOWN_ARROW_KEY_PRESSED(X) \
                                  || USB_UP_ARROW_KEY_PRESSED(X)}
#define USB_RIGHT_ARROW_KEY_PRESSED(X) \
                    ((X[0] == 0x4f) || (X[1] == 0x4f) || (X[2] == 0x4f) || (X[3] == 0x4f) || (X[4] == 0x4f) || (X[5] == 0x4f))
#define USB_LEFT_ARROW_KEY_PRESSED(X) \
                    ((X[0] == 0x50) || (X[1] == 0x50) || (X[2] == 0x50) || (X[3] == 0x50) || (X[4] == 0x50) || (X[5] == 0x50))
#define USB_DOWN_ARROW_KEY_PRESSED(X) \
                    ((X[0] == 0x51) || (X[1] == 0x51) || (X[2] == 0x51) || (X[3] == 0x51) || (X[4] == 0x51) || (X[5] == 0x51))
#define USB_UP_ARROW_KEY_PRESSED(X) \
                    ((X[0] == 0x52) || (X[1] == 0x52) || (X[2] == 0x52) || (X[3] == 0x52) || (X[4] == 0x52) || (X[5] == 0x52))
#define USB_BACKSPACE_PRESSED(X) ((X[0] == 0x2a) || (X[1] == 0x2a) || (X[2] == 0x2a) || (X[3] == 0x2a) || (X[4] == 0x2a) || (X[5] == 0x2a))
#define ARROW_KEYS_PRESSED(X) ((X.left_arrow) || (X.right_arrow) || (X.up_arrow) || (X.down_arrow))
#define ESC_PRESSED(X) (X.escape_pressed) // Assumes MAX_KEYS_PRESSED == 6
#define BACKSPACE_PRESSED(X) ((X.backspace_pressed))
#define DELAY 1000 * 250 // 250 millseconds

struct usb_keyboard_packet {
  uint8_t modifiers;
  uint8_t reserved;
  uint8_t keycode[MAX_KEYS_PRESSED];
};

/* Find and open a USB keyboard device.  Argument should point to
   space to store an endpoint address.  Returns NULL if no keyboard
   device was found. */
extern struct libusb_device_handle *openkeyboard(uint8_t *);
extern char getCharFromKeyCode(uint8_t modifier, uint8_t keycode);
extern void getCharsFromPacket(struct usb_keyboard_packet *packet, char *keys);
#endif
