/*
 *
 * CSEE 4840 Lab 2 for 2019
 *
 * Name/UNI: Please Changeto Yourname (pcy2301)
 */
#include "fbputchar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "usbkeyboard.h"
#include <pthread.h>
#include <sys/ioctl.h>
#define FBDEV "/dev/fb0"
struct winsize w;
// hardcoded max MAX_ROWS and MAX_COLS; 64 * 24

/* Update SERVER_HOST to be the IP address of
 * the chat server you are connecting to
 */
/* arthur.cs.columbia.edu */
#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000
#define BUFFER_SIZE 128

/*
 * References:
 *
 * http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 * http://www.thegeekstuff.com/2011/12/c-socket-programming/
 *
 */

int sockfd; /* Socket file descriptor */

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;
struct usb_keyboard_packet packet;
int transferred;

pthread_t network_thread_r;
pthread_t network_thread_w;
pthread_t keyboard_thread;
pthread_mutex_t keyboard_lock;

void *network_thread_f_r(void *);
void *network_thread_f_w(void *);
void *keyboard_thread_f(void *);
void fbline(char c, int row);
void fbputs(const char *s, int row, int col);
char msg_buff[MESSAGE_SIZE];
char keys[MAX_KEYS_PRESSED];
char keystate[12];

struct position text_pos = {
  .cursor_col_indx = TEXT_BOX_START_COLS,
  .cursor_row_indx = TEXT_BOX_START_ROWS,
  .msg_buff_col_indx = TEXT_BOX_START_COLS,
  .msg_buff_row_indx = TEXT_BOX_START_ROWS,
  .msg_buff_indx = 0,
  .blinking = false,
};

struct position message_pos = {
  .cursor_col_indx = MESSAGE_BOX_START_COLS,
  .cursor_row_indx = MESSAGE_BOX_START_ROWS,
  .msg_buff_col_indx = MESSAGE_BOX_START_COLS,
  .msg_buff_row_indx = MESSAGE_BOX_START_ROWS,
  .msg_buff_indx = 0,
  .blinking = false,
};

struct special_keys s_keys = {\
          .caps_lock = false,\
          .down_arrow = false,\
          .up_arrow = false,\
          .right_arrow = false,\
          .left_arrow = false,\
          .shift_pressed = false,\
          .backspace_pressed = false,\
          .escape_pressed = false,\
          .insert = false\
        };

int main()
{
  int err, col;
  struct sockaddr_in serv_addr;

  if ((err = fbopen()) != 0)
  {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }
  clearScreen();
  /* Draw MAX_ROWS of asterisks across the top and bottom of the screen */
  for (col = 0; col < MAX_COLS; col++)
  {
    fbputchar('*', 0, col);
    fbputchar('*', MAX_ROWS - 1, col);
  }
  fbputs("Hello CSEE 4840 World!", 4, 10);
  fbline('-', MAX_ROWS - 4);

  /*reset message buffers*/
  fbline(' ', MAX_ROWS - 3);
  fbline(' ', MAX_ROWS - 2);

  /* Open the keyboard */
  if ((keyboard = openkeyboard(&endpoint_address)) == NULL)
  {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }

  /* Create a TCP communications socket */
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  /* Get the server address */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if (inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0)
  {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

  /* Connect the socket to the server */
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
    exit(1);
  }
  if (pthread_mutex_init(&keyboard_lock, NULL) != 0) {
        printf("\n mutex init has failed\n");
        return 1;
    }
  
  /* Start the network thread */
  pthread_create(&network_thread_r, NULL, network_thread_f_r, NULL);
  pthread_create(&keyboard_thread, NULL, keyboard_thread_f, NULL);
  // pthread_create(&network_thread_w, NULL, network_thread_f_w, NULL);

  /* Look for and handle keypresses */
  for (;;)
  {
    //printf("Locking\n");
    pthread_mutex_lock(&keyboard_lock);
    //RESET_BACKSPACE(s_keys);
    //RESET_ARROW_KEYS(s_keys)
    //printf("Cursor pos: (rows, cols, blink): (%d, %d, %d)\n", message_pos.cursor_row_indx, message_pos.cursor_col_indx, message_pos.blinking);
    if (ESC_PRESSED(s_keys))
    { /* ESC pressed? */
      //printf("Unlocking Thread\n");
      pthread_mutex_unlock(&keyboard_lock);
      break;
    } else if (ARROW_KEYS_PRESSED(s_keys)) {
        handleArrowKeys(&message_pos, &s_keys);
    } else if (BACKSPACE_PRESSED(s_keys)) {
      handleBackSpace(&message_pos);
    } else if (USB_NOTHING_PRESSED(keys)) {
      //printf("RESETING KEYS\n");
      RESET_SPECIAL_KEYS(s_keys); // Keeps caps lock intact
      handleCursorBlink(&message_pos, &msg_buff);
      usleep(DELAY);
      handleCursorBlink(&message_pos, &msg_buff);
    }
  //printf("Unlocking\n");
  pthread_mutex_unlock(&keyboard_lock);
  usleep(DELAY);
  }

  /* Terminate the network thread */
  pthread_cancel(network_thread_r);
  pthread_cancel(keyboard_thread);
  // pthread_cancel(network_thread_w);

  /* Wait for the network thread to finish */
  pthread_join(network_thread_r, NULL);
  pthread_join(keyboard_thread, NULL);
  // pthread_join(network_thread_w, NULL);
  pthread_mutex_destroy(&keyboard_lock);
  return 0;
}

void *keyboard_thread_f(void *ignored) {
  for (;;) {
    sprintf(keystate, "%02x %02x %02x", packet.modifiers, packet.keycode[0],
                packet.keycode[1]);
    printf("%s\n", keystate);
    libusb_interrupt_transfer(keyboard, endpoint_address,
                                (unsigned char *)&packet, sizeof(packet),
                                &transferred, 0);
    if (transferred == sizeof(packet)) {
      printf("Getting lock Thread\n");
      pthread_mutex_lock(&keyboard_lock);
      getCharsFromPacket(&packet, &keys);
      setSpecialKeys(&packet, &s_keys);
      printSpecialKeys(&s_keys);
      fbputs(keystate, 6, 0);
      for (uint8_t i = 0; i < MAX_KEYS_PRESSED; i++) {
        char key = keys[i];
        if (!key)
          continue;
        /* write the char to the message buffer and print to the correct position on screen */
        if (key == '\n')
          handleEnterKey(&message_pos);
        else if (key == '\b')
          handleBackSpace(&message_pos);
        else if (key == '\t') {
          for (int i = 0; i < TAB_SPACING; i++) {
            printChar(&message_pos, &s_keys, &msg_buff, ' ');
          }
        }
        else 
          printChar(&message_pos, &s_keys, &msg_buff, key);
      }
    }
    printf("Unlocking Thread\n");
    pthread_mutex_unlock(&keyboard_lock);
  }
}

void *network_thread_f_r(void *ignored)
{
  char recvBuf[BUFFER_SIZE];
  int n;
  /* Receive data */
  while ((n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0)
  {
    recvBuf[n] = '\0';
    //printf("%s", recvBuf);
    fbPutString(recvBuf, &text_pos);
  }

  return NULL;
}

void *network_thread_f_w(void *ignored)
{

  return NULL;
}

void sendMsg () {
  if (message_pos.msg_buff_indx <= MESSAGE_SIZE - 2) {
    msg_buff[message_pos.msg_buff_indx] = '\n';
    msg_buff[message_pos.msg_buff_indx + 1] = '\0';
  } else {
    msg_buff[MESSAGE_SIZE - 2] = '\n';
    msg_buff[MESSAGE_SIZE - 1] = '\0';
  }
  printf("Message: %d %s", message_pos.msg_buff_indx, msg_buff);
  write(sockfd, msg_buff, message_pos.msg_buff_indx);
}

void printSpecialKeys(struct special_keys *s_keys) {
  char caps_insert[15];
  sprintf(caps_insert, "CAPS LOCK %d", s_keys->caps_lock);
  fbputs(caps_insert, 1, 50);

  sprintf(caps_insert, "INSERT %d", s_keys->insert);
  fbputs(caps_insert, 2, 50);

  sprintf(caps_insert, "BACKSPACE %d", s_keys->backspace_pressed);
  fbputs(caps_insert, 3, 50);

  sprintf(caps_insert, "LEFT ARROW %d", s_keys->left_arrow);
  fbputs(caps_insert, 4, 50);

  sprintf(caps_insert, "UP ARROW %d", s_keys->up_arrow);
  fbputs(caps_insert, 5, 50);

  sprintf(caps_insert, "DOWN_ARROW %d", s_keys->down_arrow);
  fbputs(caps_insert, 6, 50);

  sprintf(caps_insert, "RIGHT ARROW %d", s_keys->right_arrow);
  fbputs(caps_insert, 7, 50);
}