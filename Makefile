CC = gcc
CFLAGS = -Wall -Wextra -g -I./src
LDFLAGS = 

SERVER_SRC = src/server.c src/common.c src/db.c
CLIENT_SRC = src/client.c src/common.c

SERVER_OBJ = $(SERVER_SRC:.c=.o)
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)

all: server client

server: $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o server $(SERVER_OBJ) $(LDFLAGS)

client: $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o client $(CLIENT_OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o server client

.PHONY: all clean
