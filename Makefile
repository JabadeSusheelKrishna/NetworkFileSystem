CC = gcc
CFLAGS = -Wall -pthread

all: naming_server storage_server client

naming_server: naming_server.c utils.c trie.c lru_cache.c common.h
	$(CC) $(CFLAGS) naming_server.c utils.c trie.c lru_cache.c -o naming_server

storage_server: storage_server.c utils.c ss_file_operations.c common.h
	$(CC) $(CFLAGS) storage_server.c utils.c ss_file_operations.c -o storage_server

client: client.c utils.c common.h
	$(CC) $(CFLAGS) client.c utils.c -o client

clean:
	rm -f naming_server storage_server client *.o

rebuild: clean all
