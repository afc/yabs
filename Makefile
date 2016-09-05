CC = gcc
CFLAGS = -g -DDEBUG -Wall -ansi -O1
OBJS = scheme22.o
BIN = scheme

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

all: build

build: $(OBJS)
	$(CC) $(CFLAGS) -o $(BIN) $^
	rm -f *.o *~

rebuild: clean all

.PHONY: clean

clean:
	rm -f *.o *~ $(BIN)

