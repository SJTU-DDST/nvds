CC=gcc -std=c99
CFLAGS=-Wall -g -D_GNU_SOURCE
LDFLAGS= -libverbs

all: common

common: common.o
	${CC} -o $@ $^ ${LDFLAGS}

common.o: common.c common.h

clean:
	rm -f common common.o

