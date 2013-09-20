#include <stdio.h>
#include <stdlib.h>


/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}
