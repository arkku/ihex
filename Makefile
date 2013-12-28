CC=clang
CFLAGS=-Wall -std=c99 -pedantic -Wextra -Os

OBJS = kk_ihex_write.o kk_ihex_read.o
BINS = bin2ihex ihex2bin

.PHONY: all clean distclean test

all: $(BINS)

$(OBJS): kk_ihex.h
kk_ihex_write.o: kk_ihex_write.h
kk_ihex_read.o: kk_ihex_read.h

bin2ihex: bin2ihex.c kk_ihex_write.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+

ihex2bin: ihex2bin.c kk_ihex_read.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+

test: bin2ihex ihex2bin
	./ihex_test $<

clean:
	rm -f $(OBJS)

distclean: | clean
	rm -f $(BINS)

