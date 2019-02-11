CC=gcc
CFLAGS= -std=c99 -Wall -Wextra -Werror -g
PROJ=server

build:  $(PROJ).c $(PROJ).h
	$(CC) $(CFLAGS) -DNDEBUG $(PROJ).c -o $(PROJ)

run:  build
	@./server $(port)

clean:
	rm $(PROJ)
