SRC= \
	adbfuse.c

CC ?= gcc
OBJ = $(SRC:.c=.o)
BIN = adbfuse
CFLAGS += `pkg-config fuse --cflags` -Wall -Wextra -O0 -g #-Werror
LDFLAGS += `pkg-config fuse --libs`

all:$(BIN)

.PHONY:clean mrproper

$(BIN):$(OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJ)

mrproper:clean
	rm -f $(BIN)
