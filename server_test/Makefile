CC=/usr/bin/gcc

CFLAGS=-Wall -I../ -O2
LIBS=-lpthread

server_test : main.c ../tcpc.c ../tcpc.h ../pt.h ../packits/packits.c \
	../packits/packits.h
	gcc -o $@ $(CFLAGS) $(LIBS) main.c ../tcpc.c ../packits/packits.c

clean:
	rm -f server_test
