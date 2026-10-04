CC = gcc

CFLAGS = -Wall -Wextra -std=c11 -D_FILE_OFFSET_BITS=64
CFLAGS += $(shell pkg-config fuse3 --cflags)

LDFLAGS = $(shell pkg-config fuse3 --libs)

TARGET = fusefs

SRC = src/main.c \
      src/fuse_ops.c \
      src/inode.c \
      src/file.c

OBJ = $(SRC:.c=.o)

INCLUDE = -Iinclude

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

clean:
	rm -f $(OBJ)
	rm -f $(TARGET)

test:
	./tests/test_fuse.sh

.PHONY: all clean test
