CC=gcc -std=c99
CFLAGS=-Wall -g -D_GNU_SOURCE
LDFLAGS= -libverbs -lrdmacm

all: common

common: common.o
	${CC} -o $@ $^ ${LDFLAGS}

ud: ud.o
	${CC} -o $@ $^ ${LDFLAGS}

ud.o: ud.c

common.o: common.c common.h

clean:
	rm -f common common.o
