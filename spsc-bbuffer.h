/*
  Essentially a port of the SPSC BipBuffer queue (written in Rust), with some minor adjustments.
*/

/* TODO, in general
 *  [x] try out dark magic mmapping for buffer space
 *     [x] fix pointer arithmetic w.r.t. slices and other
 *  [x] plan out r/w synchronization (i suspect we may need semaphores here)
 *  [x] implement commit function
 *  [?] implement thread split function
 *  [x] implement buffer slice function
 *  [x] implement release function
 *  [x] cleanup writebuffer
 *  [x] implement cleanup function
 *  [x] fix datatype usage (BipBuffer struct Initialization, slices, etc...)
 *  [] better interface for interacting with read/write buffers
 *  [?] are the read/write 'certificates' necessary for synchronization?
 *  [x] write tests
 *     [x] basic usage
 *     [x] multithreaded
 *  [] write more tests
 *  [x] Makefile
 * */


/*  Example Usage (Single Thread)
 *  BipBuffer* b = new_buffer(len,uint16_t);
 *  BipPC* bpc = split(b);
 *  WritableBuff* wb = reserve_exact(bps->prod, 10);
 *  uint16_t* temp[10] = {1,2,3,3,4,5,1,2,3,4,5};
 *
 *  //the writable buffer contains ptr offsets that we should memcpy into
 *  //abstracted here by copy_into
 *  copy_into(wb);
 *  commit(wb,10,len);
 *  ReadableBuff* rb = read_data(bpc->con);
 *
 * //perform something with rb here
 *
 *
 */

/*  Example Usage (Multi-Threaded)
 *  pthread_t write_thread, read_thread;
 *  //set up bipbuffer here
 *  .
 *  .
 *  .
 *  //set up read/write functions
 *  .
 *  .
 *  .
 *  int write_retval = pthread_create(&write_thread, write_data_func, (struct args*) write_args);
 *  int read_retval  = pthread_create(&read_thread, read_data_func, (struct args*) read_args);
 *
 *  pthread_join(&write_thread);
 *  pthread_join(&read_thread);
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

#include "buffer_internal.h"

typedef struct BipBuffer {
  uint16_t* buffer;
  uint16_t buffer_len;

  atomic_int_fast16_t write; //where next byte should be written
  atomic_int_fast16_t read;  //where next byte should be read

  atomic_int_fast16_t last; // last readable block
  atomic_int_fast16_t reserve; // marks current reserved bytes

  atomic_bool read_in_prog;
  atomic_bool write_in_prog;
} BipBuffer;

BipBuffer* new_buffer(uint16_t len){
  BipBuffer* b = malloc(sizeof(BipBuffer));

  uint16_t* temp_buffer = (uint16_t*)mmap_init_buffer(len);

  b->buffer = temp_buffer;
  b->buffer_len = len;
  b->write = 0;
  b->read = 0;
  b->last = 0;
  b->reserve = 0;
  b->read_in_prog = false;
  b->write_in_prog = false;

  return b;
}

/*
   producer and consumer "objects"

   they both get access to the same bipbuffer

   TODO:
      [] Turn this into a union
*/

typedef struct BipProducer {
  BipBuffer* buff;
  uint16_t* data;
} BipProducer;

typedef struct BipConsumer {
  BipBuffer* buff;
  uint16_t* data;
} BipConsumer;

//a tuple for returning from functions
typedef struct BipPC {
  BipProducer* prod;
  BipConsumer* cons;
} BipPC;

//create producer/consumer "objects" and return them
BipPC* split(BipBuffer* b){
  BipPC* bpc = malloc(sizeof(BipPC));

  BipProducer* p = malloc(sizeof(BipProducer));
  p->buff = b;

  BipConsumer* c = malloc(sizeof(BipConsumer));
  c->buff = b;

  bpc->prod = p;
  bpc->cons = c;

  return bpc;
}

typedef struct BufferSlice {
  uint16_t head;
  uint16_t tail;
} BufferSlice;

//reserved buffer space - to be written to from e.g. an API
typedef struct WritableBuff {
  uint16_t* buff;
  BipBuffer* bipbuff;
  uint16_t to_commit;
  BufferSlice* slice;
} WritableBuff;

BufferSlice* get_buffer_slice_offsets(uint16_t start, uint16_t size){
  BufferSlice* bs = malloc(sizeof(BufferSlice));

  bs->head = start;
  bs->tail = start+size;

  return bs;
}

/*
 * TODO:
 *  [] fix pointer offset indexing here, should basically return a head/tail pointer
*/
uint16_t* get_buffer_slice(BipBuffer* b, uint16_t start, uint16_t size){
  uint16_t* ret_buffer = malloc(size*sizeof(uint16_t));
  memcpy(ret_buffer, &b->buffer+start, size*sizeof(uint16_t));
  return ret_buffer;
}

/* TODO, reserve_exact
 *       [] Better error handling
 *       [x] fix atomic variable handling
 */

//returns a usable buffer with exactly size bits available
WritableBuff* reserve_exact(BipProducer* prod, uint16_t size){
  if(atomic_exchange(&prod->buff->write_in_prog, true)){
	printf("ATOMIC_ERR\n");
	exit(EXIT_FAILURE);
  }

  BipBuffer* b = prod->buff;

  atomic_int_fast16_t write = atomic_load(&b->write);
  atomic_int_fast16_t read = atomic_load(&b->read);
  uint16_t max = size;

  bool inverted = write < read ? true : false;

  atomic_int_fast16_t start;

  if(inverted){
	if ((write + size) < read){
	  start = write;
	}
	else{ //no room, exit
	  atomic_store(&b->write_in_prog, false);
	  exit(EXIT_FAILURE);
	}
  }else{
	if((write + size) <= max)
	  start = write;
	else{
	  //we need to invert
	  if (size < read)
		start = 0;
	  else{
		//no space
		atomic_store(&b->write_in_prog, false);
		exit(EXIT_FAILURE);
	  }
	}
  }

  atomic_store(&b->reserve,start+size);

  //return usable buffer
  WritableBuff* wb = malloc(sizeof(WritableBuff));
  wb->bipbuff = b;
  wb->to_commit = 0;

  //uint16_t* temp_buff = get_buffer_slice(wb->bipbuff,start,size);
  //wb->buff = temp_buff;

  BufferSlice* b_slice = get_buffer_slice_offsets(start,size);
  wb->slice = b_slice;
  atomic_store(&b->write_in_prog, false);
  return wb;

}

uint16_t buffer_min(uint16_t x, uint16_t y){
  return x >= y ? x : y;
}

void commit(WritableBuff* wb,uint16_t used, uint16_t size){
  if(atomic_exchange(&wb->bipbuff->write_in_prog, true)){
	printf("ATOMIC_ERR\n");
	exit(EXIT_FAILURE);
  }

  BipBuffer* b = wb->bipbuff;

  uint16_t len = b->buffer_len;
  uint16_t buff_used = buffer_min(len,used);
  atomic_int_fast16_t write = atomic_load(&b->write);

  atomic_fetch_sub(&b->reserve, len-buff_used);

  uint16_t max = size;
  atomic_int_fast16_t last = atomic_load(&b->last);
  atomic_int_fast16_t new_write = atomic_load(&b->reserve);

  if(new_write<write && write != max){
	atomic_store(&b->last,write);
  }
  else if(new_write > last){
	atomic_store(&b->last,max);
  }

  atomic_store(&b->write, new_write);
  atomic_store(&b->write_in_prog, false);

  //cleanup writable buffer
  //free(wb->buff);
  free(wb);

  return;
}

//commited buffer space - to be passed to e.g. an API for reading
typedef struct ReadableBuff {
  uint16_t* buff;
  BipBuffer* bipbuff;
  uint16_t to_commit;
  BufferSlice* slice;
} ReadableBuff;

//returns a buffer ready for consumption
ReadableBuff* read_data(BipConsumer* con){
  if(atomic_exchange(&con->buff->read_in_prog, true))
	exit(EXIT_FAILURE);

  BipBuffer* b = con->buff;

  atomic_int_fast16_t write = atomic_load(&b->write);
  atomic_int_fast16_t last = atomic_load(&b->last);
  atomic_int_fast16_t read = atomic_load(&b->read);

  if(read == last && write < read){
	read = 0;
	atomic_store(&read,0);
  }

  uint16_t size;

  if(write < read)
	size = last;  //inverted
  else
	size = write; //not inverted

  size -= read;

  //printf("read: %i\n",read);
  //printf("size: %i\n",size);

  if(size == 0){
	atomic_store(&b->read_in_prog, false);
  }

  //return buffer with commited data
  ReadableBuff* rb = malloc(sizeof(ReadableBuff));
  rb->bipbuff = b;
  rb->to_commit = 0;

  //uint16_t* temp_buff = get_buffer_slice(rb->bipbuff,read,size);
  //rb->buff = temp_buff;

  BufferSlice* b_slice = get_buffer_slice_offsets(read,size);
  rb->slice = b_slice;

  return rb;
}

void release_data(ReadableBuff* rb, uint16_t used){
  uint16_t min_used = buffer_min(used,rb->bipbuff->buffer_len);

  atomic_int_fast16_t r_i_p = atomic_load(&rb->bipbuff->read_in_prog);

  if(!r_i_p)
	return;

  //assert used <= rb->bipbuff->buffer_len

  //increment next read byte
  atomic_fetch_add(&rb->bipbuff->read, min_used);

  atomic_store(&rb->bipbuff->read_in_prog,false);

  //free without freeing the bip_buffer object
  //free(rb->buff);
  free(rb);

  return;
}

void cleanup_bipbuffer(BipBuffer* b){
  //free(b->buffer);
  free(b);
  return;
}
