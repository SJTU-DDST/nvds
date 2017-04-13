CXX = clang++ -std=c++14 -O0
LD = clang++
CXXINC = -I./ -I../json/src/
CXXFLAGS = -g -Wall -D_XOPEN_SOURCE -D_GNU_SOURCE $(CXXINC)
LDFLAGS = -lrt -lpthread -lssl -lcrypto
LDLIBS = -libverbs -lboost_system

SRCS = allocator.cc         \
	   basic_server.cc		\
       client.cc			\
	   common.cc			\
	   config.cc			\
	   coordinator.cc		\
	   hash_table.cc		\
	   index.cc				\
	   infiniband.cc		\
	   message.cc			\
	   object.cc			\
	   server.cc			\
	   session.cc			\
	   tablet.cc			\
	   transport.cc

OBJS_DIR = build/
OBJS = $(addprefix $(OBJS_DIR), $(SRCS:.cc=.o))

GTEST_DIR = ../googletest/googletest
GTEST_FLAGS = -isystem ${GTEST_DIR}/include -pthread
$(OBJS_DIR)libgtest.a:
	$(CXX) $(GTEST_FLAGS) -I${GTEST_DIR} -c ${GTEST_DIR}/src/gtest-all.cc -o $(OBJS_DIR)gtest-all.o
	ar -rv $@ $(OBJS_DIR)gtest-all.o

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
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) -o $@ -c test_infiniband.cc
test_infiniband: $(OBJS_DIR)test_infiniband.o $(OBJS_DIR)infiniband.o $(OBJS_DIR)libgtest.a
	$(LD) $(LDFLAGS) $(GTEST_FLAGS) -o $(OBJS_DIR)$@ $^ $(LDLIBS)

$(OBJS_DIR)test_allocator.o: test_allocator.cc allocator.h
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) -o $@ -c test_allocator.cc
test_allocator: $(OBJS_DIR)test_allocator.o $(OBJS_DIR)allocator.o $(OBJS_DIR)libgtest.a
	$(LD) $(LDFLAGS) $(GTEST_FLAGS) -o $(OBJS_DIR)$@ $^ $(LDLIBS)

$(OBJS_DIR)test_message.o: test_message.cc message.h
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) -o $@ -c test_message.cc
test_message: $(OBJS_DIR)test_message.o $(OBJS_DIR)message.o $(OBJS_DIR)libgtest.a
	$(LD) $(LDFLAGS) $(GTEST_FLAGS) -o $(OBJS_DIR)$@ $^ $(LDLIBS)

.PHONY: clean

clean-objs:
	@rm -f $(OBJS_DIR)*.o

clean:
	@rm -rf $(OBJS_DIR)
