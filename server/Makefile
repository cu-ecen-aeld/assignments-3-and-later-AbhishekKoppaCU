# Set default compiler (Native or Cross)
CROSS_COMPILE ?= 

CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g -pthread
LDFLAGS ?=
EXECUTABLE = aesdsocket

SRC = aesdsocket.c
OBJ = aesdsocket.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS)$(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(EXECUTABLE) $(OBJ)

# .PHONY ensures these targets are always executed when called
.PHONY: all clean
