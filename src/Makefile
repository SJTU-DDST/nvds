TARGET = nvds
CC = g++ -std=c++11
LD = g++

SRCS = coordinator.cc	\
	   index.cc			\
	   infiniband.cc	\
	   nvds.cc			\
	   object.cc		\
	   server.cc		\
	   tablet.cc

CFLAGS = -g -Wall -D_XOPEN_SOURCE -D_GNU_SOURCE -I./
LDFLAGS = -libverbs -lrt -lpthread -lssl -lcrypto

OBJS_DIR = build/
OBJS = $(addprefix $(OBJS_DIR), $(SRCS:.cc=.o))

default: all

all:
	@mkdir -p $(OBJS_DIR)
	@make $(TARGET)

$(TARGET): $(OBJS)
	g++ -o $(OBJS_DIR)$@ $^ $(LDFLAGS)

$(OBJS_DIR)%.o: %.cc %.h
	$(CC) $(CFLAGS) -o $@ -c $<

infiniband.o: infiniband.cc infiniband.h
test_infiniband.o: test_infiniband.cc infiniband.h

test_infiniband: infiniband.o test_infiniband.o
	$(LD) -o $@ $^ $(LDFLAGS)

.PHONY: clean

clean:
	@rm -rf $(OBJS_DIR)
	@rm -f *.o
