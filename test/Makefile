TARGET = test
CC = gcc -std=c11
SRCS = ../src/consistent_hash.c	test_consistent_hash.c	\
	   ../src/nvds.c			test_nvds.c				\
	   ../src/rbtree.c			test_rbtree.c			\
	   test.c

CFLAGS = -g -Wall -D_XOPEN_SOURCE -D_GNU_SOURCE -I./ -I../src/
CFLAGS_MD5 = -lssl -lcrypto

OBJS_DIR = build/
OBJS = $(OBJS_DIR)consistent_hash.o	$(OBJS_DIR)test_consistent_hash.o	\
	   $(OBJS_DIR)nvds.o			$(OBJS_DIR)test_nvds.o				\
	   $(OBJS_DIR)rbtree.o			$(OBJS_DIR)test_rbtree.o			\
	   $(OBJS_DIR)test.o

default: all

all:
	@mkdir -p $(OBJS_DIR)
	@make $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(OBJS_DIR)$@ $^ $(CFLAGS_MD5)

$(OBJS_DIR)consistent_hash.o: ../src/consistent_hash.c ../src/consistent_hash.h
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJS_DIR)nvds.o: ../src/nvds.c ../src/nvds.h
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJS_DIR)rbtree.o: ../src/rbtree.c ../src/rbtree.h
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJS_DIR)test_consistent_hash.o: test_consistent_hash.c 
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJS_DIR)nvds.o: ../src/nvds.c ../src/nvds.h
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJS_DIR)rbtree.o: ../src/rbtree.c ../src/rbtree.h
	$(CC) $(CFLAGS) -o $@ -c $<

test_consistent_hash: $(OBJS_DIR)test_consistent_hash.o $(OBJS_DIR)consistent_hash.o $(OBJS_DIR)rbtree.o $(OBJS_DIR)test.o
	$(CC) -o $(OBJS_DIR)$@ $^ $(CFLAGS_MD5)

.PHONY: clean

clean:
	@rm -rf $(OBJS_DIR)
