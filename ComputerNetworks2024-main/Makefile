CC=gcc
CFLAGS=-Wall -Wextra -pedantic -ggdb3

COMPILE_FILE=main.c

all: main

main: $(COMPILE_FILE) 
	$(CC) $(CFLAGS) $^ -o $@

%.o : %.c 
	$(CC) $(CFLAGS) -c $<


.PHONY: clean
clean:
	rm -f *.o main
	rm -f main
