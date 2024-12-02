ifeq ($(CROSS_COMPILE), aarch64-none-linux-gnu-)
	CC := $(CROSS_COMPILE)gcc
else
	CC := cc
endif

all: aesdsocket

aesdsocket: aesdsocket.o
	$(CC) -o aesdsocket aesdsocket.o

aesdsocket.o: aesdsocket.c
	$(CC) -c aesdsocket.c

clean:
	rm -f aesdsocket *.o
