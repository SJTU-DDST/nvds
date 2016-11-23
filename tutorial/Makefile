CC=gcc
CFLAGS=-Wall -g -D_GNU_SOURCE
LDFLAGS= -libverbs

all: rdma

rdma: rdma.o
	${CC} -o $@ $^ ${LDFLAGS}

rdma.o: rdma.c

clean:
	rm -f rdma rdma.o

