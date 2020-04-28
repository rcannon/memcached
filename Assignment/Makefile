CXX=g++-8
CXXFLAGS=-Wall -Wextra -pedantic -Werror -std=c++17 -g -O0 
LDFLAGS=$(CXXFLAGS)
LIBS=-pthread
OBJ=$(SRC:.cc=.o)

all:  cache_server benchmark test_cache_client test_cache_lib test_evictors

cache_server: cache_server.o cache_lib.o lru_evictor.o fifo_evictor.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

benchmark: benchmark.o WorkloadGenerator.o cache_client.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

test_cache_client: test_cache_client.o cache_client.o catch.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

test_cache_lib: test_cache_lib.o cache_lib.o catch.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

test_evictors: test_evictors.o fifo_evictor.o lru_evictor.o catch.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

catch.o: catch.cc catch.hpp
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -c -o $@ $<

%.o: %.cc %.hh
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -c -o $@ $<
	
clean:
	rm -rf *.o test_cache_client test_cache_lib test_evictors cache_server benchmark

test: all
	./test_cache_lib
	./test_evictors
	echo "test_cache_client must be run manually against a running server"

valgrind: all
	valgrind --leak-check=full --show-leak-kinds=all ./test_cache_lib
	valgrind --leak-check=full --show-leak-kinds=all ./test_evictors
