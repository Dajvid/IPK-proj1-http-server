CC=gcc
CFLAGS= -std=c99 -Wall -Wextra -g

build: dyn_buffer.o server.o
	$(CC) $(CFLAGS) -o server dyn_buffer.o server.o

run: build
	@./server $(port)

dyn_buffer.o: dyn_buffer.c dyn_buffer.h
	$(CC) -c $(CFLAGS) dyn_buffer.c

server.o: server.c server.h
	$(CC) -c $(CFLAGS) server.c

clean:
	rm *.o
