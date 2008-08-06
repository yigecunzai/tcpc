CC=/usr/bin/gcc

CFLAGS=-Wall -I../ -O2
LIBS=-lpthread

test_program : main.c ../tcpc.c ../tcpc.h
	gcc -o $@ $(CFLAGS) $(LIBS) main.c ../tcpc.c

clean:
	rm -f test_program
