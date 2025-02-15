# Compiler settings
CC = gcc
CFLAGS = -O3 -march=native -flto -ffast-math -Wall -Wextra -Wpedantic
LDFLAGS = -flto -s

# Debug flags (use 'make DEBUG=1' for debug build)
ifdef DEBUG
	CFLAGS = -O0 -g -Wall -Wextra -Wpedantic
	LDFLAGS =
endif

# Project files
SRC = ccodemerge.c
OBJ = $(SRC:.c=.o)
TARGET = ccodemerge

# Default target
all: $(TARGET)

# Linking
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

# Compilation
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Additional targets
.PHONY: clean debug

clean:
	rm -f $(OBJ) $(TARGET)

debug:
	$(MAKE) DEBUG=1

# Install target (optional)
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/
