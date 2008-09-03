CC=/usr/bin/gcc

CFLAGS=-Wall -I../ -O2
LIBS=-lpthread

packits_test : test.c packits.c packits.h
	gcc -o $@ $(CFLAGS) $(LIBS) test.c packits.c

clean:
	rm -f packits_test
