#include "../spsc-bbuffer.h"
#include <stdio.h>

//hello bipbuff
int main(){
  BipBuffer* b = new_buffer(4096); // N.B. minimum page size
  printf("Hello world\n");
  printf("buffer of len:\t%i \n",b->buffer_len);
  return 0;
}
