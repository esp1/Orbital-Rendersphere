#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "debug.h"
#include "drawing.h"
#include "strip-map.h"
#include "timing.h"
#include "x2-server.h"


// externs
char panels[3][PANEL_SIZE];
int draw_idx = 0;
int to_draw_idx = 0;
int fill_idx;


ledscape_t * leds;
uint32_t x_offset = 0;
float brightness = 0;
float contrast = 1;


void drawing_init() {
  leds = ledscape_init(NUM_PIXELS_PER_STRIP);
}

void *drawing_func() {
  unsigned int frame_num = 0;

  while (keepalive) {
    // set draw index from to-draw index
    pthread_mutex_lock(&lock);
    draw_idx = to_draw_idx;
    pthread_mutex_unlock(&lock);

    new_frame = false;
    uint64_t start_usec = gettime();
    for (unsigned int slice_idx = 0; slice_idx < NUM_SLICES; slice_idx++) {
      if (new_frame || !keepalive) {
        break;
      }

      uint64_t end_time_usec = start_usec + ((slice_idx + 1) * display_interval_usec);
#if DEBUG
      printf("%d now %" PRIu64 ", end %" PRIu64 ", diff %" PRIu64 "\n", slice_idx, start_usec, end_time_usec, end_time_usec - start_usec);
#endif

      // alternate frame buffers on each draw command
      frame_num = (frame_num + 1) % 2;
      ledscape_frame_t * const frame = ledscape_frame(leds, frame_num);

      // copy panel.frame -> frame
      for (int strip_idx = 0; strip_idx < LEDSCAPE_NUM_STRIPS; strip_idx++) {
        unsigned int row = strip_idx / 4;  // range: 0-6
        unsigned int col = (4 - strip_idx) % 4;  // range: 0-3

        unsigned int y_offset = row * NUM_PIXELS_PER_STRIP;
        for (unsigned int pixel_idx = 0; pixel_idx < NUM_PIXELS_PER_STRIP; pixel_idx++) {
          // get RGB data from panel
          unsigned int y = y_offset + (row < 3 ? pixel_idx : NUM_PIXELS_PER_STRIP - 1 - pixel_idx);  // invert pixel_idx for lower hemisphere
          unsigned int x = (NUM_SLICES - 1 - (x_offset + slice_idx + (col * QUADRANT_WIDTH))) % NUM_SLICES;
          uint8_t r = panels[draw_idx][(((y * NUM_SLICES) + x) * PIXEL_SIZE) + 1];
          uint8_t g = panels[draw_idx][(((y * NUM_SLICES) + x) * PIXEL_SIZE) + 2];
          uint8_t b = panels[draw_idx][(((y * NUM_SLICES) + x) * PIXEL_SIZE) + 3];
          
          r = (r * contrast) + brightness;
          g = (g * contrast) + brightness;
          b = (b * contrast) + brightness;

          ledscape_set_color(frame, strip_map[strip_idx], pixel_idx, r, g, b);
        }
      }

      // draw frame
      ledscape_wait(leds);
      ledscape_draw(leds, frame_num);

      // wait until end of frame
      uint64_t now_usec = gettime();
      while (now_usec < end_time_usec && !new_frame) {
        now_usec = gettime();
      }
    }
  }

  // blank all strips
  ledscape_frame_t * const frame = ledscape_frame(leds, frame_num);
  for (unsigned int strip_idx = 0; strip_idx < LEDSCAPE_NUM_STRIPS; strip_idx++)
    for (unsigned int pixel_idx = 0; pixel_idx < NUM_PIXELS_PER_STRIP; pixel_idx++)
      ledscape_set_color(frame, strip_idx, pixel_idx, 0, 0, 0);
  ledscape_wait(leds);
  ledscape_draw(leds, frame_num);

  ledscape_close(leds);

  printf("Exiting drawing thread\n");
  return NULL;
}

uint32_t set_x_offset(uint32_t value) {
  x_offset = value;
#if DEBUG
  printf("x offset: %d\n", x_offset);
#endif
  return x_offset;
}

float set_brightness(float value) {
  brightness = value;
#if DEBUG
  printf("brightness: %f\n", brightness);
#endif
  return brightness;
}

float set_contrast(float value) {
  contrast = value;
#if DEBUG
  printf("contrast: %f\n", contrast);
#endif
  return contrast;
}
