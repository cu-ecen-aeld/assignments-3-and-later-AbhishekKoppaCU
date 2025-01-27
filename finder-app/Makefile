# Set default cross compiler or native compiler
CC ?= gcc
CROSS_COMPILE ?= 
CROSS_PREFIX = $(CROSS_COMPILE)
CC = $(CROSS_PREFIX)gcc

# Set the name of the target application
TARGET = writer

# Set the source file
SRC = /home/abhishekkoppa/AESD_assignments/assignment-1-AbhishekKoppaCU/writer.c

# Object files
OBJ = $(SRC:.c=.o)

# Default target to build the writer application
all: $(TARGET)

# Link object files to create the final executable
$(TARGET): $(OBJ)
	$(CC) -o $@ $^

# Rule for compiling C source files into object files
%.o: %.c
	$(CC) -c $< -o $@

# Clean up generated files
clean:
	rm -f $(TARGET) *.o

# Phony targets
.PHONY: all clean
