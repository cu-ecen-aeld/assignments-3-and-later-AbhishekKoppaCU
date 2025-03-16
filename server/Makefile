CROSS_COMPILE = aarch64-none-linux-gnu- 

CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror -g -pthread
LDFLAGS =
TARGET := aesdsocket
SRC := aesdsocket.c
OBJ := $(SRC:.c=.o)

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS)$(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(target) $(OBJ)
