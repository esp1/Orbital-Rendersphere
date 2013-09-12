/*
 * The X2 display server for the Orbital Rendersphere.
 * Listens on a specified port for image data, and displays that data to the
 * Orbital Rendersphere POV display.
 *
 * usage: x2-display <port>
 */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "drawing.h"
#include "timing.h"
#include "x2-server.h"


#define DEFAULT_PORT 10000


void INThandler() {
  printf("Interrupt!\n");
  keepalive = false;
}


int main(int argc, char **argv) {
  int port = DEFAULT_PORT;

  // check command line args
  if (argc == 1) {
    port = DEFAULT_PORT;
  } else if (argc == 2) {
    port = atoi(argv[1]);
  } else {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // initialize
  drawing_init();
  timing_init();

  signal(SIGINT, INThandler);
  pthread_mutex_init(&lock, NULL);

  // start timing thread
  pthread_t timing_thread;
  pthread_create(&timing_thread, NULL, timing_func, NULL);

  // start drawing thread
  pthread_t drawing_thread;
  pthread_create(&drawing_thread, NULL, drawing_func, NULL);

  // start server on main thread
  server_func(port);

  // shutdown
  printf("Waiting for other threads to complete\n");
  pthread_join(timing_thread, NULL);
  pthread_join(drawing_thread, NULL);

  printf("Program completed. Exiting.\n");
  pthread_exit(NULL);
}
