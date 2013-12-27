CC=clang
CFLAGS=-Wall -std=c99 -pedantic -Wextra

OBJS = kk_ihex_write.o kk_ihex_read.o
BINS = bin2ihex ihex2bin

.PHONY: all clean distclean

all: $(BINS)

$(OBJS): kk_ihex.h

bin2ihex: bin2ihex.c kk_ihex_write.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(CFLAGS_WRITEONLY) -o $@ $+

ihex2bin: ihex2bin.c kk_ihex_read.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(CFLAGS_READONLY) -o $@ $+

clean:
	rm -f $(OBJS)

distclean: | clean
	rm -f $(BINS)

