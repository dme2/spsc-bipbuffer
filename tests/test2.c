#include "../spsc-bbuffer.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

//Tests basic usage
int main(){
  printf("running!\n");
  BipBuffer* b = new_buffer(10);
  BipPC* bpc = split(b);

  WritableBuff* wb = reserve_exact(bpc->prod,10);
  uint16_t temp_src[] = {1,2,3,4,5,6,7,8,9,10};

  memcpy(wb->buff,temp_src,10);
  printf("%i\n",wb->buff[1]);
  printf("memcpy succ\n");
  commit(wb,10,b->buffer_len);
  printf("commit succ\n");
  ReadableBuff* rb = read_data(bpc->cons);

  for(int i = 0; i<10;i++)
	printf("%i\n", rb->buff[i]);

  return 0;
}

