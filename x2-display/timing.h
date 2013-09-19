#ifndef _timing_h_
#define _timing_h_

#include <inttypes.h>
#include <stdbool.h>


extern bool new_frame;
extern uint64_t display_interval_usec;
extern double rps;  // rotations per second


extern uint64_t gettime();
extern void timing_init();
extern void *timing_func();


#endif
