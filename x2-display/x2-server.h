#ifndef _x2_server_h_
#define _x2_server_h_

#include <pthread.h>
#include <stdbool.h>


extern pthread_mutex_t lock;
extern bool keepalive;


extern void *server_func(int port);


#endif
