CC = g++
CFLAGS = -std=c++11 -Werror -Wall -pedantic
GPBCONFIG = `pkg-config --cflags --libs protobuf`
PQXXCONFIG = -lpqxx -lpq

main: main.cpp world_ups.o UA.o
	$(CC) $(CFLAGS) -pthread main.cpp world_ups.o UA.o -o ups $(PQXXCONFIG) $(GPBCONFIG)

world_ups.o: world_ups.pb.cc
	$(CC) $(CFLAGS) world_ups.pb.cc -c -o world_ups.o $(GPBCONFIG)

UA.o: UA.pb.cc
	$(CC) $(CFLAGS) UA.pb.cc -c -o UA.o $(GPBCONFIG)

clean:
	rm *.o ups
