#include <inttypes.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "constants.h"
#include "debug.h"
#include "drawing.h"
#include "err.h"
#include "gpio.h"
#include "x2-server.h"


#define MAX_DISPLAY_INTERVAL_USEC (USEC_PER_SECOND / 10)
#define POLL_TIMEOUT (3 * 1000)  // 3 seconds


// externs
bool new_frame = true;
uint64_t display_interval_usec = MAX_DISPLAY_INTERVAL_USEC;
double rps = 0.0;


unsigned int hall_sensor_gpio = 61;  // gpio1_29 = 32 + 29


uint64_t gettime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * USEC_PER_SECOND) + tv.tv_usec;
}

void timing_init() {
  gpio_export(hall_sensor_gpio);
  gpio_set_dir(hall_sensor_gpio, 0);
  gpio_set_edge(hall_sensor_gpio, "rising");
}

void *timing_func() {
  printf("Listening for hall sensor on gpio %d\n", hall_sensor_gpio);

  int gpio_fd = gpio_fd_open(hall_sensor_gpio);

  struct pollfd fdset[1];
  char *buf[GPIO_MAX_BUF];

  uint64_t start_rotation_time_usec = 0;

  while (keepalive) {
    memset((void*)fdset, 0, sizeof(fdset));
    fdset[0].fd = gpio_fd;
    fdset[0].events = POLLPRI;

    // blocking read of gpio pin for hall effect sensor
    poll(fdset, 1, POLL_TIMEOUT);

    if (fdset[0].revents & POLLPRI) {
      int n = read(fdset[0].fd, buf, GPIO_MAX_BUF);
      if (n < 0)
	error("ERROR reading from gpio");
#if DEBUG
      printf("poll() GPIO interrupt - rotation timing %" PRIu64 "\n", display_interval_usec);
#endif

      // GPIO interrupt occurred - calculate rotation timing
      new_frame = true;
      uint64_t now_usec = gettime();

      uint64_t rotation_usec = now_usec - start_rotation_time_usec;
      display_interval_usec = rotation_usec / NUM_SLICES;
      if (display_interval_usec > MAX_DISPLAY_INTERVAL_USEC)
        display_interval_usec = MAX_DISPLAY_INTERVAL_USEC;
      rps = ((double) USEC_PER_SECOND) / (display_interval_usec * NUM_SLICES);

      start_rotation_time_usec = now_usec;
    }
  }

  gpio_fd_close(gpio_fd);

  printf("Exiting timing thread\n");
  return NULL;
}
