CC		= g++
CFLAGS	= -g -Wall -std=c++11 -pthread
LDFLAGS	= -lprotobuf -lcurses -lpanel

%.o				: %.cpp
				$(CC) -c $(CFLAGS) $< -o $@

%.o				: %.cc
				$(CC) -c $(CFLAGS) $< -o $@

client.out		: client.o commands.pb.o
				$(CC) client.o commands.pb.o $(LDFLAGS) -o $@

test.out		: test.o
				$(CC) test.o $(LDFLAGS) -o $@

clean			:
				rm -f client.out *.o

