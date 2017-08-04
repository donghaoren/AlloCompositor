CXXFLAGS=-I/usr/local/cuda/include -std=c++11 -g
LDFLAGS=-lzmq -lglfw -lGL -L/usr/local/cuda/lib64 -lcudart -lpthread -lGLEW

all: main client

clean:
	rm -f main client

main: main.cpp common.h cuda_shared_memory.cpp
	g++ main.cpp cuda_shared_memory.cpp -o main $(CXXFLAGS) $(LDFLAGS)

client: client.cpp common.h cuda_shared_memory.cpp
	g++ client.cpp cuda_shared_memory.cpp -o client $(CXXFLAGS) $(LDFLAGS)
