CC=clang
CFLAGS=-Wall -std=c99 -pedantic

OBJS = kk_ihex.o
BINS = bin2ihex ihex2bin

all: $(BINS)

bin2ihex: bin2ihex.c kk_ihex.c
	$(CC) $(CFLAGS) $(LDFLAGS) -DKK_IHEX_DISABLE_READING -o $@ $^

ihex2bin: ihex2bin.c kk_ihex.c
	$(CC) $(CFLAGS) $(LDFLAGS) -DKK_IHEX_DISABLE_WRITING -o $@ $^

clean:
	rm -f $(OBJS) $(BINS)
