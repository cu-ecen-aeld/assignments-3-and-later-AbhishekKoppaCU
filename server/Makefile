# Set default compiler (Native or Cross)
CROSS_COMPILE ?= aarch64-none-linux-gnu-

CC = $(CROSS_COMPILE)gcc
CFLAGS = -g -Wall
EXECUTABLE = aesdsocket

SRC = aesdsocket.c
OBJ = aesdsocket.o

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(OBJ)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c -o $(OBJ) $(SRC)

clean:
	rm -f $(EXECUTABLE) $(OBJ)

# .PHONY ensures these targets are always executed when called
.PHONY: all clean
