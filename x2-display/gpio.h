#ifndef _gpio_h_
#define _gpio_h_

/****************************************************************
 * Constants
 ****************************************************************/

#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define POLL_TIMEOUT (3 * 1000) /* 3 seconds */
#define MAX_BUF 64


extern int gpio_export(unsigned int gpio);
extern int gpio_unexport(unsigned int gpio);
extern int gpio_set_dir(unsigned int gpio, unsigned int out_flag);
extern int gpio_set_value(unsigned int gpio, unsigned int value);
extern int gpio_get_value(unsigned int gpio, unsigned int *value);
extern int gpio_set_edge(unsigned int gpio, char *edge);
extern int gpio_fd_open(unsigned int gpio);
extern int gpio_fd_close(int fd);

#endif
