CC=clang
CFLAGS=-Wall -std=c99 -pedantic

OBJS = kk_ihex.o
BINS = bin2ihex bbin2ihex

all: $(BINS)

bin2ihex: bin2ihex.c $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

bbin2ihex: bbin2ihex.c $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(OBJS) $(BINS)
