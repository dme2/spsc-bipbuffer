#include "../spsc-bbuffer.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <pthread.h>
#include <semaphore.h>
/* test multithreaded usage */

pthread_mutex_t rw_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int cond_count = 0;

sem_t buffer_sync_sem;

uint16_t temp_src[] = {1,2,3,4,5,6,7,8,9,10};
void* write_data_function(void* bpc){
  pthread_mutex_lock(&rw_mutex);
  BipPC* real_bpc = (BipPC*) bpc;

  WritableBuff* wb = reserve_exact(real_bpc->prod,10);

  memcpy(wb->bipbuff->buffer + wb->slice->head, temp_src, 20);
  printf("memcpy success\n");

  commit(wb,10,wb->bipbuff->buffer_len);
  pthread_mutex_unlock(&rw_mutex);
  //sem post(&buffer_sync_sem);
  cond_count++;
  pthread_cond_signal(&cond);
}

void* read_data_function(void* bpc){
  pthread_mutex_lock(&cond_mutex);
  //sem wait(&buffer_sync_sem);
  //terrible, use a sem here
  while(cond_count < 1){
	pthread_cond_wait(&cond,&cond_mutex);
  }
  pthread_mutex_unlock(&cond_mutex);

  pthread_mutex_lock(&rw_mutex);
  BipPC* real_bpc = (BipPC*) bpc;

 ReadableBuff* rb = read_data(real_bpc->cons);
  printf("head: %i\ntail: %i\n",rb->slice->head,rb->slice->tail);

  for(int i = 0; i<10;i++){
	printf("%i:\t",i);
	printf("%i\n", rb->bipbuff->buffer[i+rb->slice->head]);
  }

  release_data(rb,20);
  //cleanup_bipbuffer(rb->bipbuff);

  pthread_mutex_unlock(&rw_mutex);
}

int main(){
  printf("running!\n");

  //sem_init(buffer_sync_sem, 0,1);
  pthread_t write_thread,read_thread;
  BipBuffer* b = new_buffer(4096);
  BipPC* bpc = split(b);

  pthread_create(&write_thread, NULL, write_data_function, (void*)bpc);

  pthread_create(&read_thread, NULL, read_data_function, (void*)bpc);
  /*
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
  */
  pthread_join(write_thread,NULL);
  pthread_join(read_thread,NULL);

  //release_data(bpc->rb,20);
  cleanup_bipbuffer(b);
  return 0;
}
