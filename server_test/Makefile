CC=/usr/bin/gcc

CFLAGS=-Wall -I../ -O2
LIBS=-lpthread

all : server_test test_client
server_test : main.c ../tcpc.c ../tcpc.h ../pt.h ../packits/packits.c \
	../packits/packits.h ../ll.h
	gcc -o $@ $(CFLAGS) $(LIBS) -pg main.c ../tcpc.c ../packits/packits.c

test_client : test_client.c ../tcpc.c ../tcpc.h ../pt.h ../ll.h
	gcc -o $@ $(CFLAGS) $(LIBS) test_client.c ../tcpc.c

clean:
	rm -f server_test test_client
