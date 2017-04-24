CC		= g++
CFLAGS	= -O3 -std=c++11 -pthread
LDFLAGS	= -lprotobuf -lncursesw -lpanelw

%.o				: %.cpp
				$(CC) -c $(CFLAGS) $< -o $@

%.o				: %.cc
				$(CC) -c $(CFLAGS) $< -o $@

client.out		: client.o commands.pb.o
				$(CC) -o $@ client.o commands.pb.o $(LDFLAGS)

test.out		: test.o
				$(CC) -o $@ test.o $(LDFLAGS)

clean			:
				rm -f client.out *.o

