CXXFLAGS=-I/usr/local/cuda/include
LDFLAGS=-lglfw -lGL -L/usr/local/cuda/lib -lcudart

all: main client


main: main.cpp common.h
	g++ main.cpp -o main $(LDFLAGS)

client: client.cpp common.h
	g++ client.cpp -o client $(CXXFLAGS)
