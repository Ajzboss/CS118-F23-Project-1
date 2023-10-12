CC=gcc
SRC=server.cpp
OBJ=server
CCFLAGS=-Wall --std=c++17

all: $(OBJ)

proxy: $(SRC)
	$(CC) $(CCFLAGS) $< -o $@

clean:
	rm -rf $(OBJ)

.PHONY: all clean
