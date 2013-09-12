/** \file
 * Test the ledscape library by pulsing RGB on the first three LEDS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>
#include "ledscape.h"
#include "strip-map.h"

static void
ledscape_fill_color(
	ledscape_frame_t * const frame,
	const unsigned num_pixels,
	const uint8_t r,
	const uint8_t g,
	const uint8_t b
)
{
	for (unsigned i = 0 ; i < num_pixels ; i++)
		for (unsigned strip = 0 ; strip < LEDSCAPE_NUM_STRIPS ; strip++)
			ledscape_set_color(frame, strip, i, r, g, b);
}

int main (void)
{
	const int num_pixels = 17;
	ledscape_t * const leds = ledscape_init(num_pixels);
	time_t last_time = time(NULL);
	unsigned last_i = 0;

	struct timespec tim, tim2;
	tim.tv_sec = 0;
	tim.tv_nsec = 50*1000*1000;

	unsigned i = 0;
	while (1)
	{
		// Alternate frame buffers on each draw command
		const unsigned frame_num = i++ % 2;
		ledscape_frame_t * const frame
			= ledscape_frame(leds, frame_num);

		uint8_t val = i >> 1;
		ledscape_fill_color(frame, num_pixels, 0, 0, 0);

		for (unsigned strip_idx = 0 ; strip_idx < LEDSCAPE_NUM_STRIPS ; strip_idx++)
		{
			for (unsigned pixel_idx = 0; pixel_idx <= strip_idx % 10; pixel_idx++) {
				uint8_t r = ((strip_idx / 10) == 0) ? 100 : 0;
				uint8_t g = ((strip_idx / 10) == 1) ? 100 : 0;
				uint8_t b = ((strip_idx / 10) == 2) ? 100 : 0;
				int strip = strip_map[strip_idx];
				if (strip >= 0)
					ledscape_set_color(frame, strip, pixel_idx, r, g, b);
			}
/*
			for (unsigned pixel_idx = 0 ; pixel_idx < num_pixels ; pixel_idx++)
			{
				ledscape_set_color(
					frame,
					strip_idx,
					pixel_idx,
#if 1
					((strip_idx % 3) == 0) ? (i) : 0,
					((strip_idx % 3) == 1) ? (i) : 0,
					((strip_idx % 3) == 2) ? (i) : 0
#else
					((strip_idx % 3) == 0) ? 100 : 0,
					((strip_idx % 3) == 1) ? 100 : 0,
					((strip_idx % 3) == 2) ? 100 : 0
#endif
				);
				ledscape_set_color(frame, strip_idx, 3*pixel_idx+1, 0, pixel_idx+val + 80, 0);
				ledscape_set_color(frame, strip_idx, 3*pixel_idx+2, 0, 0, pixel_idx+val + 160);
			}
*/
		}
		// do some work
		//nanosleep(&tim, &tim2);

		// wait for the previous frame to finish;
		const uint32_t response = ledscape_wait(leds);
		time_t now = time(NULL);
		if (now != last_time)
		{
			printf("%d fps. starting %d previous %"PRIx32"\n",
				i - last_i, i, response);
			last_i = i;
			last_time = now;
		}

		ledscape_draw(leds, frame_num);
	}

	ledscape_close(leds);

	return EXIT_SUCCESS;
}
