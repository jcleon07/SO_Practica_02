CC = gcc
CFLAGS = -Wall -O2 -Ilib/xxhash

all: ui search

ui: ui.c
	$(CC) $(CFLAGS) ui.c -o ui

search: search.c  
	$(CC) $(CFLAGS) search.c hash.c lib/xxhash/xxhash.c -o search 

clean:
	rm -f ui search 

.PHONY: all clean