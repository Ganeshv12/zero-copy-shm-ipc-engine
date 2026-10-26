CC = gcc
CFLAGS = -Wall -Wextra -O2 -pedantic
LDLIBS = -pthread -lrt

all: ipc_server ipc_client

ipc_server: ipc_server.c master_ipc_bus.h
	$(CC) $(CFLAGS) ipc_server.c -o ipc_server $(LDLIBS)

ipc_client: ipc_client.c master_ipc_bus.h
	$(CC) $(CFLAGS) ipc_client.c -o ipc_client $(LDLIBS)

clean:
	rm -f ipc_server ipc_client *.o
