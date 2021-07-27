#include "../spsc-bbuffer.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <pthread.h>
/* test multithreaded usage */

int main(){
  printf("running!\n");
  BipBuffer* b = new_buffer(4096);
  BipPC* bpc = split(b);

  WritableBuff* wb = reserve_exact(bpc->prod,20); //need to reserve more than 10 for some reason???
  uint16_t temp_src[] = {1,2,3,4,5,6,7,8,9,10};

  memcpy(wb->bipbuff->buffer + wb->slice->head, temp_src, 20);
  printf("memcpy success\n");
  commit(wb,10,b->buffer_len);
  printf("commit success\n");
  ReadableBuff* rb = read_data(bpc->cons);
  printf("head: %i\ntail: %i\n",rb->slice->head,rb->slice->tail);

  for(int i = 0; i<10;i++){
	printf("%i:\t",i);
	printf("%i\n", rb->bipbuff->buffer[i+rb->slice->head]);
  }

  release_data(rb,20);
  cleanup_bipbuffer(b);
  return 0;
}
