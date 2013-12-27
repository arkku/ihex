CC=clang
CFLAGS=-Wall -std=c99 -pedantic

OBJS = kk_ihex.o kk_ihex_write.o kk_ihex_read.o
BINS = bin2ihex ihex2bin

CFLAGS_WRITEONLY=-DIHEX_DISABLE_READING -DIHEX_LINE_MAX_LENGTH=32
CFLAGS_READONLY=-DIHEX_DISABLE_WRITING

.PHONY: all clean distclean

all: $(BINS)

$(OBJS): kk_ihex.h

kk_ihex_write.o: kk_ihex.c
	$(CC) $(CFLAGS) $(CFLAGS_WRITEONLY) -c -o $@ $<

kk_ihex_read.o: kk_ihex.c
	$(CC) $(CFLAGS) $(CFLAGS_READONLY) -c -o $@ $<

bin2ihex: bin2ihex.c kk_ihex_write.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(CFLAGS_WRITEONLY) -o $@ $+

ihex2bin: ihex2bin.c kk_ihex_read.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(CFLAGS_READONLY) -o $@ $+

clean:
	rm -f $(OBJS)

distclean: | clean
	rm -f $(BINS)

