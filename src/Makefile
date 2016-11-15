TARGET = nvds
CC = gcc -std=c11
SRCS = hash/consistent_hash.c 		\
	   core/nvds.c core/rbtree.c

CFLAGS = -g -Wall -D_XOPEN_SOURCE -D_GNU_SOURCE -Icore/ -Ihash/ -Inode/ -Irdma/
CFLAGS_MD5 = -lssl -lcrypto

OBJS_DIR = build/
OBJS = $(addprefix $(OBJS_DIR), $(SRCS:.c=.o))

default: all

all:
	@mkdir -p $(OBJS_DIR)
	@mkdir -p $(OBJS_DIR)core
	@mkdir -p $(OBJS_DIR)hash
	@mkdir -p $(OBJS_DIR)node
	@mkdir -p $(OBJS_DIR)rdma
	@make $(TARGET)

$(TARGET): $(OBJS)
	gcc -o $(OBJS_DIR)$@ $^ $(CFLAGS_MD5)

$(OBJS_DIR)%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean

clean:
	@rm -rf $(OBJS_DIR)
