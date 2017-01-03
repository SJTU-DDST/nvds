CXX = clang++ -std=c++11
LD = clang++
CXXFLAGS = -g -Wall -D_XOPEN_SOURCE -D_GNU_SOURCE -I./
LDFLAGS = -lrt -lpthread -lssl -lcrypto
LDLIBS = -libverbs

SRCS = client.cc			\
	   common.cc			\
	   coordinator.cc		\
	   hash_table.cc		\
	   index.cc				\
	   infiniband.cc		\
	   message.cc			\
	   object.cc			\
	   server.cc			\
	   tablet.cc			\
	   transport.cc

OBJS_DIR = build/
OBJS = $(addprefix $(OBJS_DIR), $(SRCS:.cc=.o))

#depend: .depend
#.depend: $(SRCS)
#	@rm -f ./.depend
#	@$(CXX) $(CFLAGS) -MM $^ -MF  ./.depend;
#include .depend

default: all

all:
	@mkdir -p $(OBJS_DIR)
	@make coordinator
	@make server
	@make client

client: $(OBJS) $(OBJS_DIR)client_main.o
	$(LD) $(LDFLAGS) -o $(OBJS_DIR)$@ $^ $(LDLIBS)

server: $(OBJS) $(OBJS_DIR)server_main.o
	$(LD) $(LDFLAGS) -o $(OBJS_DIR)$@ $^ $(LDLIBS)

coordinator: $(OBJS) $(OBJS_DIR)coordinator_main.o
	$(LD) $(LDFLAGS) -o $(OBJS_DIR)$@ $^ $(LDLIBS)

$(OBJS_DIR)%.o: %.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $^
	
$(OBJS_DIR)coordinator_main.o: coordinator_main.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $^

$(OBJS_DIR)server_main.o : server_main.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $^

$(OBJS_DIR)client_main.o : client_main.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $^

$(OBJS_DIR)test_infiniband.o: test_infiniband.cc infiniband.h
	$(CXX) $(CXXFLAGS) -o $@ -c $^

test_infiniband: $(OBJS_DIR)test_infiniband.o $(OBJS_DIR)infiniband.o
	$(LD) $(LDFLAGS) -o $(OBJS_DIR)$@ $^ $(LDLIBS)

.PHONY: clean

clean-objs:
	@rm -f $(OBJS_DIR)*.o

clean:
	@rm -rf $(OBJS_DIR)