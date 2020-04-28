CC = g++
CFLAGS = -std=c++11 -Werror -Wall -pedantic -Wextra
GPBCONFIG = `pkg-config --cflags --libs protobuf`
PQXXCONFIG = -lpqxx -lpq

main: main.cpp ups.o world_ups.o UA.o
	$(CC) $(CFLAGS) -pthread main.cpp ups.o world_ups.o UA.o -o main $(PQXXCONFIG) $(GPBCONFIG)

world_ups.o: world_ups.pb.cc
	$(CC) $(CFLAGS) world_ups.pb.cc -c -o world_ups.o $(GPBCONFIG)

UA.o: UA.pb.cc
	$(CC) $(CFLAGS) UA.pb.cc -c -o UA.o $(GPBCONFIG)

ups.o: UPS.cpp
	$(CC) $(CFLAGS) -pthread UPS.cpp -c -o ups.o $(PQXXCONFIG) $(GPBCONFIG)

clean:
	rm *.log *.o main
