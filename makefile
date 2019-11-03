CC = gcc
CFLAGS =

default: help

1: clean client server1
	@echo ""
	@echo " * V1 built *"
	rm -f *.o \
	rm -f p1/*.o \

2: clean client server2
	@echo ""
	@echo " * V2 built *"
	rm -f *.o \
	rm -f p2/*.o \

3: clean client server3
	@echo ""
	@echo " * V3 built *"
	rm -f *.o \
	rm -f p3/*.o


clean:
	rm -f *.o \
	rm -f p1/*.o \
	rm -f p2/*.o \
	rm -f p3/*.o

client: client.o
	$(CC) $(CFLAGS) -o client client.o

server1: p1/server.o p1/messages.o p1/client_functions.o
	$(CC) $(CFLAGS) -o server p1/server.o p1/messages.o p1/client_functions.o

server2: p2/server.o p2/messages.o p2/client_functions.o
	$(CC) $(CFLAGS) -o server p2/server.o p2/messages.o p2/client_functions.o

server3: p3/server.o p3/messages.o p3/client_functions.o
	$(CC) $(CFLAGS) -pthread -o server p3/server.o p3/messages.o p3/client_functions.o

help:
	@echo " * invalid command, use: *"
	@echo " * 'make 1' to build V1  *"
	@echo " * 'make 2' to build V2  *"
	@echo " * 'make 3' to build V3  *"
