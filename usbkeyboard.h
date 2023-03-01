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
#define USB_ESC_PRESSED(X) (X.escape_pressed) // Assumes MAX_KEYS_PRESSED == 6
#define USB_NOTHING_PRESSED(X) ((!X[0]) && (!X[1]) && (!X[2]) && (!X[3]) && (!X[4]) && (!X[5]))
#define USB_ARROW_KEYS_PRESSED(X) ((X.left_arrow) || (X.right_arrow) || (X.up_arrow) || (X.down_arrow))
//#define USB_DECLARE_SPECIAL_KEYS(X) {\
          .caps_lock = false,\
          .down_arrow = false,\
          .up_arrow = false,\
          .right_arrow = false,\
          .left_arrow = false,\
          .shift_pressed = false,\
          .backspace_pressed = false,\
          .escape_pressed = false\
        }\

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
