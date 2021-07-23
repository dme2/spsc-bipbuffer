#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>

#include <linux/memfd.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/types.h>

/* lifted from this very cool "dark magic" circular queue implementation:
     https://github.com/tmick0/toy-queue.git
 */

//#if __GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 27)
static inline int memfd_create(const char *name, unsigned int flags){
  return syscall(__NR_memfd_create, name, flags);
}
//#endif

void* mmap_init_buffer(size_t sz){
  //enusre that our size is a multiple of a pagesize
  if(sz % getpagesize() != 0){
	printf("page size error\n");
	exit(EXIT_FAILURE);
  }

  //create an anonymous file
  int fd=0;
  if((fd = memfd_create("queue_region",0)) == -1){
	printf("fd error");
	exit(EXIT_FAILURE);
  }

  //set the buffer size
  if((ftruncate(fd,sz)) !=0){
	printf("fd trunc error");
	exit(EXIT_FAILURE);
  }

  //get buffer address
  void* buffer;
  if((buffer = mmap(NULL, 2*sz, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED){
	printf("buffer address error");
	exit(EXIT_FAILURE);
  }

  if(mmap(buffer, sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0) == MAP_FAILED){
	printf("buffer region 1 error\n");
	exit(EXIT_FAILURE);
   }
  if(mmap(buffer+sz, sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0) == MAP_FAILED){
	printf("buffer region 2 error\n");
	exit(EXIT_FAILURE);
  }

  return buffer;
}

void destroy_buffer(void* buffer,size_t sz){
  munmap(buffer + sz, sz);
  return;
}
