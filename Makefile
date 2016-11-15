TARGET = nvds
CC = gcc -std=c11
SRCS = 

CFLAGS = -g -Wall -D_XOPEN_SOURCE -D_GNU_SOURCE -I./

OBJS_DIR = build/
OBJS = $(addprefix $(OBJS_DIR), $(SRCS:.c=.o))

default: all

all:
	@mkdir -p $(OBJS_DIR)
	@make $(TARGET)

$(TARGET): $(OBJS)
	gcc -o $(OBJS_DIR)$@ $^

$(OBJS_DIR)%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean

clean:
	@rm -rf $(OBJS_DIR)
