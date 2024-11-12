ifeq ($(CROSS_COMPILE), aarch64-none-linux-gnu-)
	CC := /opt/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc
else
	CC := cc
endif

all: writer

writer: writer.o
	$(CC) -o writer writer.o

writer.o: writer.c
	$(CC) -c writer.c

clean:
	rm -f writer *.o
