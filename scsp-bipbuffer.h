/* *
 * single consumer, single producer bip buffer is defined here
 *
 * What is a bip(artite) buffer?
 *   the concept is similiar to a circular buffer - data is to fill the buffer in a (semi) continous fashion. This results in
 *   synchronization difficulties with regards to reads/writes. In order to overcome these difficulties we maintain two pointers
 *   on our fixed size buffer. This typically works well for certain applications - but most of the time we're not looking to
 *   pass two pointers around in our program, we want a solid continuous block of data. This is where the bipbuffer comes in
 *   it allows us to maintain a continous block of memory. The bip part comes from it being bipartate (i.e. composed of two
 *   regions, typically denoted A and B).
 *
 *   The main idea of the bipbuffer is as follows:
 *      space is reserved (this denotes the region as region A or region B) (reserve)
 *      data is inserted in the buffer such that is remains continues (commit)
 *      data can the be read and (or) decommited from the buffer, freeing up space in that region
 *
 * Why SCSP?
 *   because reserving, storing, and reading from a buffer are all potentially expensive and error prone operations, especially
 *   in something like audio programming where reads and writes happen constantly and missed data can result in bad souding audio
 *   - we want 1. speed and 2. consistancy. We can get speed from splitting theese jobs between threads. One produces and the
 *   other consumes. And we can get consistancy from ensuring that we're not sending bad data (if we don't get the data in time
 *   should we send silence??) and we can potentially do that by blocking until we have at least the correct amount of data.
 *
 * General Procedure:
 *   Two seperate threads, in each;
 *     a mutex for locking while reading/writing
 *     a  semaphore for signaling
 *
 * e.g.
 *
 *   bip_buffer b = init_buffer(); //should spin up a writer and reader thread for the buffer
 *   uint16_t* data = get_test_data();

 *   //reserve space -> write/commit data to it
 *   uint8_t res = try_write_and_commit(b, data); //this should be performed via the write thread
 *   //read data and clear reserved region
 *   uint16_t* res_data = try_read_and_drop(b);   //this should be performed via the read thread
 *
 *
 * */


/*
 *  TODO:
 *     [] figure out writing function
 *        e.g. in alsa/tinyalsa, the function pcm_readi(&buffer) takes a reference to the buffer
 *             so should we pass in a handle/pointer to the buffers current reserved space?
 *
 *             reserve(buff);
 *             pcm_readi(&buffer + buff->reserved_space);
 *             commit(buffer, read_size);
 *             do_something_with_buffer_data(buffer);
 *             decommit(buffer, read_size);
 *
 *     [] fix reserve/commit/decommit logic - reserve should return a pointer to the newly reserved block
 *         commit should clear the reservation and leave only commit_sized data in the block. which is then able to be read
 *         decommit should clear teh commited data
 */

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

//our pthreads for reading and writing to our bip-buffer
typedef struct rw {
  pthread_t write_thread;
  pthread_t read_thread;
} rw;

pthread_mutex_t write_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t read_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct bip_buffer{
  uint32_t capacity;
  uint32_t readsize;
  uint32_t reserved_head; /* NB: reads will be from reserved_head to reserved_head+readsize */
  uint32_t reserved_tail;

  uint16_t* buffer;
  uint16_t* read_buffer;

  //regions
  uint32_t a_head;
  uint32_t a_tail;
  uint32_t b_head;
  uint32_t b_tail;

  sem_t* sem;

} bip_buffer;

typedef struct threadargs{
  bip_buffer *b;
  void* data;
  uint16_t rw_size;
} threadargs;

void* temp_read_fn(void* arg);

int try_write_and_commit(threadargs* ta);

/*
rw* init_threads(){
  rw* rw_threads = malloc(sizeof(rw));
  pthread_create(&read_thread, NULL, temp_write_fn,NULL);

  rw_threads->write_thread = write_thread;
  rw_threads->read_thread = read_thread;

  return rw_threads;
}
*/

bip_buffer* init_buffer(uint32_t capacity, uint32_t readsize){
  uint16_t* buff = calloc(0,capacity * sizeof(uint16_t));
  bip_buffer* b = malloc(sizeof(bip_buffer));
  sem_t* sem = malloc(sizeof(sem_t));
  int ret = sem_init(sem,0,0);

  if (ret){
	exit(1);
  }

  b->capacity = capacity;
  b->readsize = readsize;
  b->reserved_head = 0;
  b->reserved_tail = 0;
  b->buffer = buff;
  b->a_head = 0;
  b->a_tail = 0;
  b->b_head = 0;
  b->b_tail = 0;
  b->sem = sem;

  return b;
}

uint32_t commit_min(uint32_t x, uint32_t y){
  return x >= y ? y : x;
}

int16_t* read_data(bip_buffer* b){
  int16_t* ret_buff = calloc(0, b->a_tail-b->a_head);

  memcpy(&ret_buff, &b->buffer[b->a_head], b->readsize);
  return ret_buff;
}

uint8_t reserve(bip_buffer* b){
  uint32_t rsrv_head;
  uint32_t free_space = b->b_head - b->a_head;

  if (free_space > 0)
	rsrv_head = b->b_tail;
  else{
	uint32_t a_space = b->capacity - b->a_tail;
	if(a_space >= b->a_head){
	  rsrv_head = b->a_head;
	  free_space = a_space;
	}else{
	  free_space = b->a_head;
	  rsrv_head = 0;
	}
  }
  uint32_t rsrv_len = commit_min(free_space, b->capacity);
  b->reserved_head = rsrv_head;
  b->reserved_tail = rsrv_head + rsrv_len;
  return 0;
}

//comit commit_size blocks on b returns 0 if successful, 1 otherwise
uint8_t commit(bip_buffer* b, uint32_t commit_size){
  //if commit length > reserved_space...go with amt of reserved_spae
  uint32_t to_commit = commit_min(commit_size, b->reserved_head - b->reserved_tail);

  if ((b->a_tail - b->a_head == 0) && (b->b_tail - b->b_head == 0)){
	b->a_head = b->reserved_head;
	b->a_tail = b->reserved_head + to_commit;
  } else if (b->reserved_head == b->a_tail){
	b->a_tail += to_commit;
  }else
	b->b_tail += to_commit;

  b->reserved_head = 0;
  b->reserved_tail = 0;
  return 0;
}

uint8_t decommit(bip_buffer* b, uint32_t size){
  if (size >= b->a_tail - b->a_head){
	b->a_head = b->b_head;
	b->a_tail = b-> b_tail;
	b->b_head = 0;
	b->b_tail = 0;
  }
  else
	b->a_head +=size;

  return 0;
}

/* write to buffer */
uint8_t write_data(bip_buffer* b, void* data){
  memcpy(&b->buffer[b->reserved_head], &data, b->reserved_tail * sizeof(uint16_t));
  return 0;
}

int try_write_data(bip_buffer* b, void* data){
  threadargs* ta = malloc(sizeof(threadargs));
  ta->b = b;
  ta->data = data;
  ta->rw_size = 1024;

  pthread_t write_thread;
  pthread_create(&write_thread, NULL, (void*)try_write_and_commit, ta);

  pthread_join(write_thread,NULL);
  return 0;
}

uint16_t* try_read_data(bip_buffer* b){
  threadargs* ta = malloc(sizeof(threadargs));
  ta->b = b;
  ta->rw_size = 1024;

  pthread_t read_thread;
  pthread_create(&read_thread, NULL, (void*)try_write_and_commit, ta);

  pthread_join(read_thread,NULL);
  return b->read_buffer;
}

//reserve space -> write data -> commit
int try_write_and_commit(threadargs* ta){

  //sem wait?
  //mutex lock?

  reserve(ta->b);

  pthread_mutex_lock(&write_mutex);

  write_data(ta->b,ta->data);
  commit(ta->b, ta->rw_size);

  pthread_mutex_unlock(&write_mutex);
  //sem_post(ta->b->sem);

  return 0;
}

//read from reserved space (reserved_head + readsize) -> decommit -> return buffer
void try_read_and_drop(threadargs* ta){
  bip_buffer* b = ta->b;
  uint16_t* ret_buffer = calloc(0, b->a_tail - b->a_head);

  pthread_mutex_lock(&read_mutex);

  memcpy(&ret_buffer, &b->buffer[b->a_head], b->a_tail * sizeof(uint16_t));

  pthread_mutex_unlock(&read_mutex);

  b->read_buffer = ret_buffer;

  decommit(b,ta->rw_size);

  return;
}
