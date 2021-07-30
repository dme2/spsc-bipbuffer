CFLAGS=-std=gnu11 
LDFLAGS=-Wall -lpthread
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

test1: $(T1SRC)
	$(CC) -o tests/$@ tests/test1.c $(LDFLAGS) $(CFlAGS) 

test2: $(T2SRC)
	$(CC) -o tests/$@ tests/test2.c $(LDFLAGS) $(CFlAGS) 

test3: $(T3SRC)
	$(CC) -o tests/$@ tests/test3.c $(LDFLAGS) $(CFlAGS) 

tests: test1 test2 test3
	

$(TOBJ1): spsc-bbuffer.h
	$(CC) -o tests/test1.o $(LDFLAGS) $(CFLAGS)

$(TOBJ2): spsc-bbuffer.h
	$(CC) -o tests/test2.o $(LDFLAGS) $(CFLAGS)

$(TOBJ3): spsc-bbuffer.h

$(OBJS): spsc-bbuffer.h

clean:
	rm -f *.o *~ tmp* main 

clean-tests:
	rm -f tests/test1 tests/test2 tests/test3

.PHONY: test clean
