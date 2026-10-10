CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra

spsc_queue: spsc_queue.cpp
	$(CXX) $(CXXFLAGS) -pthread $< -o $@

clean:
	rm -f spsc_queue
