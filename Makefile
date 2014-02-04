CC=clang
CFLAGS=-Wall -std=c99 -pedantic -Wextra -Os #-emit-llvm
LDFLAGS=-Os
AR=ar
ARFLAGS=rcs

OBJS = kk_ihex_write.o kk_ihex_read.o bin2ihex.o ihex2bin.o
BINS = bin2ihex ihex2bin libkk_ihex.a

.PHONY: all clean distclean test

all: $(BINS)

$(OBJS): kk_ihex.h
bin2ihex.o kk_ihex_write.o: kk_ihex_write.h
ihex2bin.o kk_ihex_read.o: kk_ihex_read.h

libkk_ihex.a: kk_ihex_write.o kk_ihex_read.o
	$(AR) $(ARFLAGS) $@ $+

bin2ihex: bin2ihex.o kk_ihex_write.o
	$(CC) $(LDFLAGS) -o $@ $+

ihex2bin: ihex2bin.o kk_ihex_read.o
	$(CC) $(LDFLAGS) -o $@ $+

test: bin2ihex ihex2bin
	./ihex_test $<

clean:
	rm -f $(OBJS)

distclean: | clean
	rm -f $(BINS)

