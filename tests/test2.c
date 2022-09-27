#include "../spsc-bbuffer.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* Test basic usage */
int main(){
  printf("running!\n");
  BipBuffer* b = new_buffer(4096);
  BipPC* bpc = split(b);

  WritableBuff* wb = reserve_exact(bpc->prod, 10);
  int16_t temp_src[] = {1,2,3,4,5,6,7,8,9,10};

  memcpy(wb->bipbuff->buffer + wb->slice->head, temp_src, sizeof(int16_t)*10);
  printf("memcpy success\n");
  commit(wb,10,b->buffer_len);
  printf("commit success\n");
  ReadableBuff* rb = read_data(bpc->cons);
  printf("head: %i\ntail: %i\n",rb->slice->head,rb->slice->tail);

  for(uint16_t i = 0; i<10;i++){
	printf("%i:\t",i);
	printf("%i\n", rb->bipbuff->buffer[i+rb->slice->head]);
  }

  release_data(rb,10);
  cleanup_bipbuffer(b);
  return 0;
}
