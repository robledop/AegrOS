CC=gcc
CFLAGS=-c -Wall -O0
LIBS =

all: bmp

bmp: main.o
	$(CC) main.o $(LIBS) -o bmp

main.o: main.c
	$(CC) $(CFLAGS) main.c

clean:
	rm -rf *.o bmp
