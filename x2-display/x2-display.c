/* 
 * The X2 display server for the Orbital Rendersphere.
 * Listens on a specified port for image data, and displays that data to the
 * Orbital Rendersphere POV display.
 *
 * usage: x2-display <port>
 */

#include <arpa/inet.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "gpio.h"
#include "ledscape.h"

// function prototypes
void error(char *msg);
void INThandler();
int socket_init(int portno);
void *draw_func();
void *timing_func();

// globals
pthread_mutex_t lock;
bool keepalive = true;

#define BUFSIZE 1024
#define PIXEL_SIZE 4
#define FRAME_SIZE 17 * 24 * PIXEL_SIZE
#define PANEL_SIZE 56 * FRAME_SIZE

// 3 panels, one of which is being drawn in, is to be drawn in, and is being filled
// each panel consists of 224 slices, where each slice is a horizontal line of resolution
// a frame consists of the rgb values for each of the 17 pixels in all of the 24 led strips
// each pixel takes up 4 bytes of information, stored as BRGA (but A is not used)
// each frame encompasses 4 slices

ledscape_frame_t * panels[3][224];
int draw_idx = 0;
int to_draw_idx = 0;
int fill_idx;

/*
 * Main (server) thread
 */
int main(int argc, char **argv) {
  // check command line args
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  signal(SIGINT, INThandler);
  pthread_mutex_init(&lock, NULL);

  // start timing thread
  pthread_t timing_thread;
  pthread_create(&timing_thread, NULL, timing_func, NULL);

  // start draw thread
  pthread_t drawing_thread;
  //pthread_create(&drawing_thread, NULL, drawing_func, NULL);

  // main server thread
  int listenfd = socket_init(atoi(argv[1]));

  struct sockaddr_in clientaddr;
  socklen_t clientlen = sizeof(clientaddr);

  while (1) {
    // accept: wait for a connection request
    int connfd = accept(listenfd, (struct sockaddr *) &clientaddr, &clientlen);
    if (connfd < 0)
      error("ERROR on accept");

    // read panel data length
    unsigned int datalen = 0;
    char buf[4];
    int n = read(connfd, buf, 4);
    if (n < 0) error("ERROR reading from socket");
    datalen = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
    printf("length = %d bytes\n", datalen);
    if (datalen > PANEL_SIZE)
      error("data too long!");
    else if (datalen == 0)
      break;
    
    // determine fill index
    pthread_mutex_lock(&lock);
    if (draw_idx == 0 || to_draw_idx == 0)
      if (draw_idx == 1 || to_draw_idx == 1)
	fill_idx = 2;
      else
	fill_idx = 1;
    else
      fill_idx = 0;
    pthread_mutex_unlock(&lock);

    // read panel data from the client
    bzero(panels[fill_idx], PANEL_SIZE);
    unsigned int offset = 0;
    while (offset < datalen) {
      n = read(connfd, panels[fill_idx] + offset, BUFSIZE);
      if (n < 0) error("ERROR reading from socket");
      offset += n;
    }

    // set to-draw index
    pthread_mutex_lock(&lock);
    to_draw_idx = fill_idx;
    pthread_mutex_unlock(&lock);
    
    close(connfd);
  }
}

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

void INThandler() {
  printf("Interrupt!\n");
  keepalive = false;
}

int socket_init(int portno) {
  int listenfd = socket(AF_INET, SOCK_STREAM, 0); // listening socket
  if (listenfd < 0) 
    error("ERROR opening socket");

  /*
   * setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  int optval = 1; // flag value for setsockopt
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  // build the server's internet address
  struct sockaddr_in serveraddr; // server's addr
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET; // we are using the Internet
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); // accept reqs to any IP addr
  serveraddr.sin_port = htons((unsigned short)portno); // port to listen on

  // bind: associate the listening socket with a port
  if (bind(listenfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  // listen: make it a listening socket ready to accept connection requests
  if (listen(listenfd, 5) < 0) // allow 5 requests to queue up
    error("ERROR on listen");

  return listenfd;
}

#define USEC_PER_SECOND 1000000

uint64_t gettime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * USEC_PER_SECOND) + tv.tv_usec;
}

/*
 * Draw thread
 */

bool new_frame = true;
uint64_t display_interval_usec = USEC_PER_SECOND;

void *drawing_func() {
  const unsigned int num_pixels = 17;
  ledscape_t * const leds = ledscape_init(num_pixels);
  
  unsigned int frame_num = 0;

  while (keepalive) {
    // set draw index from to-draw index
    pthread_mutex_lock(&lock);
    draw_idx = to_draw_idx;
    pthread_mutex_unlock(&lock);

    new_frame = false;
    for (unsigned int slice_idx = 0; slice_idx < 224; slice_idx++) {
      if (new_frame) {
        break;
      }

      uint64_t end_time_usec = (slice_idx + 1) * display_interval_usec;

      // alternate frame buffers on each draw command
      frame_num = (frame_num + 1) % 2;
      ledscape_frame_t * const frame = ledscape_frame(leds, frame_num);

      // copy panel.frame -> frame
      ledscape_frame_t * const pframe = panels[draw_idx][slice_idx];
      memcpy(frame, pframe, FRAME_SIZE);

      // draw frame
      ledscape_wait(leds);
      ledscape_draw(leds, frame_num);

      // wait until end of frame
      uint64_t now_usec;
      while (now_usec < end_time_usec && !new_frame) {
	now_usec = gettime();
      }
    }
  }

  ledscape_close(leds);

  printf("Exiting drawing thread\n");
  return NULL;
}

/*
 * Timing thread
 */

void *timing_func() {
  unsigned int hall_sensor_gpio = 30;
  
  gpio_export(hall_sensor_gpio);
  gpio_set_dir(hall_sensor_gpio, 0);
  gpio_set_edge(hall_sensor_gpio, "rising");
  int gpio_fd = gpio_fd_open(hall_sensor_gpio);
  
  int nfds = 1;
  struct pollfd fdset[nfds];
  int timeout = 3 * 1000;  /* 3 seconds */
  char *buf[MAX_BUF];
  
  uint64_t start_rotation_time_usec = 0;

  while (keepalive) {
    memset((void*)fdset, 0, sizeof(fdset));
    fdset[0].fd = gpio_fd;
    fdset[0].events = POLLPRI;
    
    // blocking read of gpio pin for hall effect sensor
    poll(fdset, nfds, timeout);
    
    if (fdset[0].revents & POLLPRI) {
      read(fdset[0].fd, buf, MAX_BUF);
      printf("\npoll() GPIO interrupt - rotation timing %" PRIu64 "\n", display_interval_usec);

      // GPIO interrupt occurred - calculate rotation timing
      new_frame = true;
      uint64_t now_usec = gettime();
      
      display_interval_usec = (now_usec - start_rotation_time_usec) / 224;
      if (display_interval_usec > USEC_PER_SECOND)
	display_interval_usec = USEC_PER_SECOND;
      
      start_rotation_time_usec = now_usec;
    }
  }
  
  gpio_fd_close(gpio_fd);
 
  printf("Exiting timing thread\n");
  return NULL;
}
