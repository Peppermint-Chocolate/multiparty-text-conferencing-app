CC=gcc
all:server client
server: server.c
	$(CC) -o server server.c -lm -pthread
client: client.c
	$(CC) -o client client.c -lm -pthread
clean:
	rm -f *.o server client
