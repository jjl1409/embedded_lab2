#include "usbkeyboard.h"

#include <stdio.h>
#include <stdlib.h>

/* References on libusb 1.0 and the USB HID/keyboard protocol
 *
 * http://libusb.org
 * http://www.dreamincode.net/forums/topic/148707-introduction-to-using-libusb-10/
 * http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 * http://www.usb.org/developers/devclass_docs/Hut1_11.pdf
 */

/*
 * Find and return a USB keyboard device or NULL if not found
 * The argument con
 *
 */
struct libusb_device_handle *openkeyboard(uint8_t *endpoint_address)
{
  libusb_device **devs;
  struct libusb_device_handle *keyboard = NULL;
  struct libusb_device_descriptor desc;
  ssize_t num_devs, d;
  uint8_t i, k;

  /* Start the library */
  if (libusb_init(NULL) < 0)
  {
    fprintf(stderr, "Error: libusb_init failed\n");
    exit(1);
  }

  /* Enumerate all the attached USB devices */
  if ((num_devs = libusb_get_device_list(NULL, &devs)) < 0)
  {
    fprintf(stderr, "Error: libusb_get_device_list failed\n");
    exit(1);
  }

  /* Look at each device, remembering the first HID device that speaks
     the keyboard protocol */

  for (d = 0; d < num_devs; d++)
  {
    libusb_device *dev = devs[d];
    if (libusb_get_device_descriptor(dev, &desc) < 0)
    {
      fprintf(stderr, "Error: libusb_get_device_descriptor failed\n");
      exit(1);
    }

    if (desc.bDeviceClass == LIBUSB_CLASS_PER_INTERFACE)
    {
      struct libusb_config_descriptor *config;
      libusb_get_config_descriptor(dev, 0, &config);
      for (i = 0; i < config->bNumInterfaces; i++)
        for (k = 0; k < config->interface[i].num_altsetting; k++)
        {
          const struct libusb_interface_descriptor *inter =
              config->interface[i].altsetting + k;
          if (inter->bInterfaceClass == LIBUSB_CLASS_HID &&
              inter->bInterfaceProtocol == USB_HID_KEYBOARD_PROTOCOL)
          {
            int r;
            if ((r = libusb_open(dev, &keyboard)) != 0)
            {
              fprintf(stderr, "Error: libusb_open failed: %d\n", r);
              exit(1);
            }
            if (libusb_kernel_driver_active(keyboard, i))
              libusb_detach_kernel_driver(keyboard, i);
            libusb_set_auto_detach_kernel_driver(keyboard, i);
            if ((r = libusb_claim_interface(keyboard, i)) != 0)
            {
              fprintf(stderr, "Error: libusb_claim_interface failed: %d\n", r);
              exit(1);
            }
            *endpoint_address = inter->endpoint[0].bEndpointAddress;
            goto found;
          }
        }
    }
  }

found:
  libusb_free_device_list(devs, 1);

  return keyboard;
}

void setSpecialKeys(struct usb_keyboard_packet *packet, struct special_keys *s_keys) {
  if (USB_LEFT_ARROW_KEY_PRESSED(packet->keycode))
    s_keys->left_arrow = true;
  else
    s_keys->left_arrow = false;
  if (USB_RIGHT_ARROW_KEY_PRESSED(packet->keycode))
    s_keys->right_arrow = true;
  else
    s_keys->right_arrow = false;
  if (USB_UP_ARROW_KEY_PRESSED(packet->keycode))
    s_keys->up_arrow = true;
  else
    s_keys->up_arrow = false;
  if (USB_DOWN_ARROW_KEY_PRESSED(packet->keycode))
    s_keys->down_arrow = true;
  else
    s_keys->down_arrow = false;
  if (USB_BACKSPACE_PRESSED(packet->keycode))
    s_keys->backspace_pressed = true;
  else
    s_keys->backspace_pressed = false;
  if (USB_CAPS_LOCK_PRESSED(packet->keycode))
    s_keys->caps_lock = !s_keys->caps_lock;
  if (USB_INSERT_PRESSED(packet->keycode))
    s_keys->insert = !s_keys->insert;
}

void getCharsFromPacket(struct usb_keyboard_packet *packet, char *keys) {
  for (uint8_t i = 0; i < 6; i++) {
    keys[i] = getCharFromKeyCode(packet->modifiers, packet->keycode[i]);
  }
}

char getCharFromKeyCode(uint8_t modifier, uint8_t keycode)
{
  if (keycode >= 0x04 && keycode <= 0x1d) {
      if (USB_SHIFT_PRESSED(modifier)) {
          return 'A' + (keycode - 0x04);
      } else {
          return 'a' + (keycode - 0x04);
      }
  }
  if (USB_SHIFT_PRESSED(modifier)) {
      switch (keycode) {
          case 0x1e: return '!';
          case 0x1f: return '@';
          case 0x20: return '#';
          case 0x21: return '$';
          case 0x22: return '%';
          case 0x23: return '^';
          case 0x24: return '&';
          case 0x25: return '*';
          case 0x26: return '(';
          case 0x27: return ')';
          case 0x28: return '\n';
          case 0x29: s_keys.escape_pressed = true; return 0;
          case 0x2b: return '\t';
          case 0x2c: return ' ';
          case 0x2d: return '_';
          case 0x2e: return '+';
          case 0x2f: return '{';
          case 0x30: return '}';
          case 0x31: return '|';
          case 0x33: return ':';
          case 0x34: return '"';
          case 0x35: return '~';
          case 0x36: return '<';
          case 0x37: return '>';
          case 0x38: return '?';
          default: return 0;
      }
  } else {
      switch (keycode) {
          case 0x1e: return '1';
          case 0x1f: return '2';
          case 0x20: return '3';
          case 0x21: return '4';
          case 0x22: return '5';
          case 0x23: return '6';
          case 0x24: return '7';
          case 0x25: return '8';
          case 0x26: return '9';
          case 0x27: return '0';
          case 0x28: return '\n';
          case 0x29: s_keys.escape_pressed = true; return 0;
          case 0x2b: return '\t';
          case 0x2c: return ' ';
          case 0x2d: return '-';
          case 0x2e: return '=';
          case 0x2f: return '[';
          case 0x30: return ']';
          case 0x31: return '\\';
          case 0x33: return ';';
          case 0x34: return '\'';
          case 0x35: return '`';
          case 0x36: return ','';
          case 0x37: return '.';
          case 0x38: return '/';
          default: return 0;
      }
  }
  return 0;
}