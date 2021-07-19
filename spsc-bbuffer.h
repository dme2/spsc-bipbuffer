/*
  Essentially a port of the SPSC BipBuffer queue (written in Rust), with some minor adjustments.
*/

/* TODO, in general
 *  [?] plan out r/w synchronization (i suspect we may need semaphores here)
 *  [x] implement commit function
 *  [?] implement thread split function
 *  [x]  implement buffer slice function
 *  []  implement release function
 *  []  implement cleanup function
 *  []  fix datatype usage (BipBuffer struct Initialization, slices, etc...)
 * */


/*  Example Usage
 *  BipBuffer* b = new_buffer(len,uint16_t);
 *  BipPC* bpc = split(b);
 *  WritableBuff* wb = reserve_exact(bps->prod, 10);
 *  uint16_t* temp[10] = {1,2,3,3,4,5,1,2,3,4,5};
 *  copy_into(wb->buff,temp);
 *  commit(wb,10,len);
 *  ReadableBuff* rb = read_data(bpc->con);
 *
 * //perform something with rb here...
 *
 */

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct BipBuffer {
  void* buffer;
  uint16_t buffer_len;

  atomic_int_least16_t write; //where next byte should be written
  atomic_int_least16_t read;  //where next byte should be read

  atomic_int_least16_t last; // last readable block
  atomic_int_least16_t reserve; // marks current reserved bytes

  atomic_bool read_in_prog;
  atomic_bool write_in_prog;
} BipBuffer;

BipBuffer* new_buffer(uint16_t len, void* type){
  BipBuffer* b = malloc(sizeof(BipBuffer));
  void* temp_buffer = calloc(0,sizeof(type)*len);

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
  void* data;
} BipProducer;

typedef struct BipConsumer {
  BipBuffer* buff;
  void* data;
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

//reserved buffer space - to be written to from e.g. an API
typedef struct WritableBuff {
  void* buff;
  BipBuffer* bipbuff;
  uint16_t to_commit;

} WritableBuff;

uint16_t* get_buffer_slice(BipBuffer* b, uint16_t start, uint16_t size){
  uint16_t* ret_buffer = calloc(0, size*sizeof(uint16_t));
  memcpy(ret_buffer, b->buffer+start, size*sizeof(uint16_t));
  return ret_buffer;
}

/* TODO, reserve_exact
 *       [] Better error handling
 *       [x] fix atomic variable handling
 */

//returns a usable buffer with exactly size bits available
WritableBuff* reserve_exact(BipProducer* prod, uint16_t size){
  if(atomic_exchange(&prod->buff->write_in_prog, true))
	exit(EXIT_FAILURE);

  BipBuffer* b = prod->buff;

  atomic_int_least16_t write = atomic_load(&b->write);
  atomic_int_least16_t read = atomic_load(&b->read);
  uint16_t max = size;

  bool inverted = write < read ? true : false;

  atomic_int_least16_t start;

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

  void* temp_buff = (void*)get_buffer_slice(wb->bipbuff,start,size);
  wb->buff = temp_buff;
  return wb;
}

uint16_t buffer_min(uint16_t x, uint16_t y){
  return x >= y ? x : y;
}

void commit(WritableBuff* wb,uint16_t used, uint16_t size){
  if(atomic_exchange(&wb->bipbuff->write_in_prog, true))
	exit(EXIT_FAILURE);

  BipBuffer* b = wb->bipbuff;

  uint16_t len = b->buffer_len;
  uint16_t buff_used = buffer_min(len,used);
  atomic_int_least16_t write = atomic_load(&b->write);

  atomic_fetch_sub(&b->reserve, len-buff_used);

  uint16_t max = size;
  atomic_int_least16_t last = atomic_load(&b->last);
  atomic_int_least16_t new_write = atomic_load(&b->reserve);

  if(new_write<write && write != max){
	atomic_store(&b->last,write);
  }
  else if(new_write > last){
	atomic_store(&b->last,max);
  }

  atomic_store(&b->write, new_write);
  atomic_store(&b->write_in_prog, false);

  return;
}

//commited buffer space - to be passed to e.g. an API for reading
typedef struct ReadableBuff {
  void* buff;
  BipBuffer* bipbuff;
  uint16_t to_commit;

} ReadableBuff;

//returns a buffer ready for consumption
ReadableBuff* read_data(BipConsumer* con){
  if(atomic_exchange(&con->buff->read_in_prog, true))
	exit(EXIT_FAILURE);

  BipBuffer* b = con->buff;

  atomic_int_least16_t write = atomic_load(&b->write);
  atomic_int_least16_t last = atomic_load(&b->last);
  atomic_int_least16_t read = atomic_load(&b->read);

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

  if(size == 0){
	atomic_store(&b->read_in_prog, false);
  }

  //return buffer with commited data
  ReadableBuff* rb = malloc(sizeof(ReadableBuff));
  rb->bipbuff = b;
  rb->to_commit = 0;

  void* temp_buff = get_buffer_slice(rb->bipbuff,read,size);
  rb->buff = temp_buff;
  return rb;
}

void release_data(ReadableBuff* rb);
