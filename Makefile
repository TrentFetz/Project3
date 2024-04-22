# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99

# Source files and object files
SRCDIR = src
SOURCES = $(SRCDIR)/filesys.c
OBJECTS = $(SOURCES:.c=.o)

# Target executable
TARGET = filesys

# Build target
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o $@

# Object file compilation
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(OBJECTS) $(TARGET)

# Run target
run: $(TARGET)
	./$(TARGET) $(SRCDIR)/fat.img

# Default target
.PHONY: all
all: $(TARGET)