CFLAGS=-std=gnu11 
LDFLAGS=-Wall -pthread
SRCS=$(wildcard *.c)

T1SRC=$(tests/test1.c)
T2SRC=$(tests/test2.c)
T3SRC=$(tests/test3.c)

OBJS=$(SRCS:.c=.o)

TOBJ1=$(T1SRC:.c=.o)
TOBJ2=$(T2SRC:.c=.o)
TOBJ3=$(T3SRC:.c=.o)

main: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(CFlAGS) 

test1: $(TOBJ1)
	$(CC) -o $@ $^ $(LDFLAGS) $(CFlAGS) 

test1: $(TOBJ2)
	$(CC) -o $@ $^ $(LDFLAGS) $(CFlAGS) 

test1: $(TOBJ3)
	$(CC) -o $@ $^ $(LDFLAGS) $(CFlAGS) 

$(TOBJ1): spsc-bbuffer.h
	$(CC) -o tests/test1.o $(LDFLAGS) $(CFLAGS)

$(TOBJ1): spsc-bbuffer.h
	$(CC) -o tests/test2.o $(LDFLAGS) $(CFLAGS)

$(TOBJ1): spsc-bbuffer.h
	$(CC) -o tests/test3.o $(LDFLAGS) $(CFLAGS)

$(OBJS): spsc-bbuffer.h

clean:
	rm -f *.o *~ tmp*

.PHONY: test clean
