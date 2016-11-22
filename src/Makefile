TARGET = nvds
CC = gcc -std=c99
LD = gcc
SRCS = consistent_hash.c	\
	   nvds.c rbtree.c

CFLAGS = -g -Wall -D_XOPEN_SOURCE -D_GNU_SOURCE -I./
LDFLAGS = -libverbs -lrt -lpthread
CFLAGS_MD5 = -lssl -lcrypto

OBJS_DIR = build/
OBJS = $(addprefix $(OBJS_DIR), $(SRCS:.c=.o))

default: all

all:
	@mkdir -p $(OBJS_DIR)
	@make $(TARGET)

$(TARGET): $(OBJS)
	gcc -o $(OBJS_DIR)$@ $^ $(CFLAGS_MD5)

$(OBJS_DIR)%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

infiniband.o: infiniband.c infiniband.h
test_infiniband.o: test_infiniband.c infiniband.h

test_infiniband: infiniband.o test_infiniband.o
	${LD} -o $@ $^ ${LDFLAGS}

.PHONY: clean

clean:
	@rm -rf $(OBJS_DIR)
	@rm -f *.o
