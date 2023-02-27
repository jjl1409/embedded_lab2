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
#define MESSAGE_SIZE 128
struct winsize w;
// hardcoded max rows and cols; 64 * 24

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

pthread_t network_thread_r;
pthread_t network_thread_w;
void *network_thread_f_r(void *);
void *network_thread_f_w(void *);
void fbline(char c, int row);
void fbputs(const char *s, int row, int col);
char msg_buff[MESSAGE_SIZE];
int msg_buff_indx = 0;
int msg_buff_col_indx = 0;
int msg_buff_row_indx = ROWS - 3;

int main()
{
  int err, col;

  struct sockaddr_in serv_addr;

  struct usb_keyboard_packet packet;
  int transferred;
  char keystate[12];

  if ((err = fbopen()) != 0)
  {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0; col < COLS; col++)
  {
    fbputchar('*', 0, col);
    fbputchar('*', ROWS - 1, col);
  }

  fbputs("Hello CSEE 4840 World!", 4, 10);
  fbline('-', ROWS - 4);
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

  /* Start the network thread */
  pthread_create(&network_thread_r, NULL, network_thread_f_r, NULL);
  // pthread_create(&network_thread_w, NULL, network_thread_f_w, NULL);

  /* Look for and handle keypresses */
  for (;;)
  {
    libusb_interrupt_transfer(keyboard, endpoint_address,
                              (unsigned char *)&packet, sizeof(packet),
                              &transferred, 0);
    if (transferred == sizeof(packet))
    {

      sprintf(keystate, "%02x %02x %02x", packet.modifiers, packet.keycode[0],
              packet.keycode[1]);
      printf("%s\n", keystate);
      char key = getCharFromKeyCode(&packet);
      /* write the char to the message buffer and print to the correct position on screen*/
      if (msg_buff_indx < MESSAGE_SIZE)
      {
        msg_buff[msg_buff_indx] = key;
        msg_buff_indx++;
        msg_buff_col_indx++;
        /* if we hit the end of the screen go to the next row and reset colun index*/
        if (msg_buff_col_indx == COLS)
        {
          msg_buff_col_indx = 0;
          msg_buff_row_indx++;
        }
        fbputchar((char)key, msg_buff_row_indx, msg_buff_col_indx);
      }
      fbputs(keystate, 6, 0);
      if (packet.keycode[0] == 0x29)
      { /* ESC pressed? */
        break;
      }
    }
  }

  /* Terminate the network thread */
  pthread_cancel(network_thread_r);
  // pthread_cancel(network_thread_w);

  /* Wait for the network thread to finish */
  pthread_join(network_thread_r, NULL);
  // pthread_join(network_thread_w, NULL);

  return 0;
}

void *network_thread_f_r(void *ignored)
{
  char recvBuf[BUFFER_SIZE];
  int n;
  /* Receive data */
  while ((n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0)
  {
    recvBuf[n] = '\0';
    printf("%s", recvBuf);
    fbputs(recvBuf, 8, 0);
  }

  return NULL;
}

void *network_thread_f_w(void *ignored)
{

  return NULL;
}
