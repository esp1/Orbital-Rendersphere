/*
 * The X2 display server for the Orbital Rendersphere.
 * Listens on a specified port for image data, and displays that data to the
 * Orbital Rendersphere POV display.
 *
 * usage: x2-display <port>
 */

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "debug.h"
#include "drawing.h"
#include "err.h"
#include "timing.h"


#define POLL_TIMEOUT (3 * 1000)  // 3 seconds
#define BUFSIZE 1024


// externs
pthread_mutex_t lock;
bool keepalive = true;


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
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

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

char *read_4bytes(int connfd) {
  char buf[4];
  int n = read(connfd, buf, 4);
  if (n < 0) error("ERROR reading 4 bytes from socket");
  return buf;
}

uint32_t read_uint32(int connfd) {
  char *b = read_4bytes(connfd);
  return (b[0] << 24) + (b[1] << 16) + (b[2] << 8) + b[3];
}

float read_float(int connfd) {
  char *b = read_4bytes(connfd);
  return (b[0] << 24) + (b[1] << 16) + (b[2] << 8) + b[3];
}

void write_stats(int connfd) {
  // write rotations per second back to client
  int n = write(connfd, &rps, sizeof(rps));
  if (n < 0)
    error("ERROR writing RPS to socket");

  // write frames per second back to client
  n = write(connfd, &fps, sizeof(fps));
  if (n < 0)
    error("ERROR writing FPS to socket");
}

void *server_func(int port) {
  printf("Server listening on port %d\n", port);
  int listenfd = socket_init(port);

  struct sockaddr_in clientaddr;
  socklen_t clientlen = sizeof(clientaddr);

  struct pollfd fdset[1];
  memset((void*)fdset, 0, sizeof(fdset));
  fdset[0].fd = listenfd;
  fdset[0].events = POLLIN;

  while (keepalive) {
    // poll: wait for a connection request
    poll(fdset, 1, POLL_TIMEOUT);

    if (fdset[0].revents == POLLIN) {
#if DEBUG_SERVER
      printf("Received connection\n");
#endif

      // accept connection request
      int connfd = accept(fdset[0].fd, (struct sockaddr *) &clientaddr, &clientlen);
      if (connfd < 0)
        error("ERROR on accept");

      // read command
      int n;
      char command;
      n = read(connfd, &command, 1);
      if (n < 0)
	error("ERROR reading command from socket");
      if (command == '0') {
        // read panel data length
        uint32_t datalen = read_uint32(connfd);
#if DEBUG_SERVER
        printf("length = %d\n", datalen);
#endif

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
        while (offset < datalen * 4) {
          n = read(connfd, panels[fill_idx] + offset, BUFSIZE);
          if (n < 0) error("ERROR reading panel data from socket");
          offset += n;
        }

        // set to-draw index
        pthread_mutex_lock(&lock);
        to_draw_idx = fill_idx;
        pthread_mutex_unlock(&lock);

        // write stats back to client
//        write_stats(connfd);
      } else if (command == '?') {
        // write stats back to client
        write_stats(connfd);
      } else if (command == 'x') {
        // x offset
        uint32_t value = set_x_offset(read_uint32(connfd));
//	n = write(connfd, &value, sizeof(value));
//	if (n < 0)
//	  error("ERROR writing x offset value to socket");
      } else if (command == 'b') {
        // read brightness
        float value = set_brightness(read_float(connfd));
//	n = write(connfd, &value, sizeof(value));
//	if (n < 0)
//	  error("ERROR writing brightness value to socket");
      } else if (command == 'c') {
        // read contrast
        float value = set_contrast(read_float(connfd));
//	n = write(connfd, &value, sizeof(value));
//	if (n < 0)
//	  error("ERROR writing contrast value to socket");
      }

      close(connfd);
    }
  }

  close(listenfd);

  printf("Exiting server thread\n");
  return NULL;
}
