all: server client

server: server.c common.c
	gcc -o server server.c common.c -lpthread

client: client.c common.c
	gcc -o client client.c common.c -lpthread

.PHONY: clean

clean:
	- rm server client
