CXX = g++ -std=c++14 -O3
LD = g++
CXXINC = -I./ -I../json/src/
#CXXFLAGS = -g -fsanitize=address -Wall -D_XOPEN_SOURCE -D_GNU_SOURCE $(CXXINC)
CXXFLAGS = -g -Wall -D_XOPEN_SOURCE -D_GNU_SOURCE $(CXXINC)
LDFLAGS = 
#LDLIBS = -lasan -libverbs -lboost_system -lboost_thread -lrt -lpthread -lssl -lcrypto
LDLIBS = -libverbs -lboost_system -lboost_thread -lrt -lpthread -lssl -lcrypto


SRCS = allocator.cc         \
	   basic_server.cc		\
       client.cc			\
	   common.cc			\
	   coordinator.cc		\
	   index.cc				\
	   infiniband.cc		\
	   message.cc			\
	   server.cc			\
	   session.cc			\
	   tablet.cc			\
	   MurmurHash2.cc

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
	@make coordinator server client library

coordinator: $(OBJS) $(OBJS_DIR)coordinator_main.o
	$(LD) $(LDFLAGS) -o $(OBJS_DIR)$@ $^ $(LDLIBS)

server: $(OBJS) $(OBJS_DIR)server_main.o
	$(LD) $(LDFLAGS) -o $(OBJS_DIR)$@ $^ $(LDLIBS)

client: $(OBJS) $(OBJS_DIR)client_main.o
	$(LD) $(LDFLAGS) -o $(OBJS_DIR)$@ $^ $(LDLIBS)

library: $(OBJS)
	ar rcs $(OBJS_DIR)libnvds.a $(OBJS)

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
test_infiniband: $(OBJS_DIR)test_infiniband.o $(OBJS_DIR)infiniband.o $(OBJS_DIR)common.o $(OBJS_DIR)libgtest.a
	$(LD) $(LDFLAGS) $(GTEST_FLAGS) -o $(OBJS_DIR)$@ $^ $(LDLIBS)

$(OBJS_DIR)test_allocator.o: test_allocator.cc allocator.h
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) -o $@ -c test_allocator.cc
test_allocator: $(OBJS_DIR)test_allocator.o $(OBJS_DIR)allocator.o $(OBJS_DIR)libgtest.a
	$(LD) $(LDFLAGS) $(GTEST_FLAGS) -o $(OBJS_DIR)$@ $^ $(LDLIBS)

$(OBJS_DIR)test_stl_map.o: test_stl_map.cc hash.h
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) -o $@ -c test_stl_map.cc
test_stl_map: $(OBJS_DIR)test_stl_map.o $(OBJS_DIR)MurmurHash2.o $(OBJS_DIR)libgtest.a
	$(LD) $(LDFLAGS) $(GTEST_FLAGS) -o $(OBJS_DIR)$@ $^ $(LDLIBS)

$(OBJS_DIR)test_message.o: test_message.cc message.h
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) -o $@ -c test_message.cc
test_message: $(OBJS_DIR)test_message.o $(OBJS_DIR)message.o $(OBJS_DIR)libgtest.a
	$(LD) $(LDFLAGS) $(GTEST_FLAGS) -o $(OBJS_DIR)$@ $^ $(LDLIBS)

install:
	@sudo mkdir -p /usr/include/nvds/
	@sudo cp ./*.h /usr/include/nvds/
	@sudo cp ../json/src/json.hpp /usr/include/nvds/
	@sudo cp ./build/libnvds.a /usr/lib/

.PHONY: clean

clean-objs:
	@rm -f $(OBJS_DIR)*.o

clean:
	@rm -rf $(OBJS_DIR)
