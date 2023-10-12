CC=gcc
SRC=server.c
OBJ=server
CCFLAGS=-Wall --std=c++20

all: $(OBJ)

proxy: $(SRC)
	$(CC) $< -o $@

clean:
	rm -rf $(OBJ)

.PHONY: all clean
