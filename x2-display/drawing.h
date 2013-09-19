#ifndef _drawing_h_
#define _drawing_h_

#include "ledscape.h"


/*
 * 3 panels, one of which is being drawn in, is to be drawn in, and is being filled
 * each panel consists of 224 slices, where each slice is a horizontal line of resolution
 * a frame consists of the rgb values for each of the 17 pixels in all of the 24 led strips
 * each pixel takes up 4 bytes of information, stored as BRGA (but A is not used)
 * each frame encompasses 4 slices
 */

#define NUM_PIXELS_PER_STRIP 17
#define PIXEL_SIZE 4
#define FRAME_SIZE (LEDSCAPE_NUM_STRIPS * NUM_PIXELS_PER_STRIP * PIXEL_SIZE)
#define QUADRANT_WIDTH 56
#define NUM_SLICES (QUADRANT_WIDTH * 4)
#define PANEL_SIZE (QUADRANT_WIDTH * FRAME_SIZE)


extern char panels[3][PANEL_SIZE];
extern int draw_idx;
extern int to_draw_idx;
extern int fill_idx;


extern void drawing_init();
extern void *drawing_func();
extern uint32_t set_x_offset(uint32_t value);
extern float set_brightness(float value);
extern float set_contrast(float value);


#endif
