CC=gcc
CFLAGS=-Wall -g
L_FLAGS=-fsanitize=address,undefined


all: l7-1_server l7-1_client_local l7-1_client_tcp

l7-1_server: l7-1_server.c
	$(CC) $(CFLAGS) -o l7-1_server l7-1_server.c

l7-1_client_local: l7-1_client_local.c
	$(CC) $(CFLAGS) -o l7-1_client_local l7-1_client_local.c

l7-1_client_tcp: l7-1_client_tcp.c
	$(CC) $(CFLAGS) -o l7-1_client_tcp l7-1_client_tcp.c

clean:
	rm -f l7-1_server l7-1_client_local l7-1_client_tcp
