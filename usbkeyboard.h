#ifndef _USBKEYBOARD_H
#define _USBKEYBOARD_H

#include <libusb-1.0/libusb.h>
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
#define USB_SHIFT_PRESSED(X) (( (X & (USB_LSHIFT | USB_RSHIFT)) > 0) || caps_lock)
#define USB_ESC_PRESSED(X) ((X[0] == 0x29) || (X[1] == 0x29) || (X[2] == 0x29) || (X[3] == 0x29) || (X[4] == 0x29) || (X[5] == 0x29)) // Assumes MAX_KEYS_PRESSED == 6

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
bool caps_lock = false;
#endif
