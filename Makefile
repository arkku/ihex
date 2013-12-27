CC=clang
CFLAGS=-Wall -std=c99 -pedantic

OBJS = kk_ihex.o kk_ihex_write.o kk_ihex_read.o
BINS = bin2ihex ihex2bin

all: $(BINS)

$(OBJS): kk_ihex.h

kk_ihex_write.o: kk_ihex.c
	$(CC) $(CFLAGS) -DKK_IHEX_DISABLE_READING -c -o $@ $<

kk_ihex_read.o: kk_ihex.c
	$(CC) $(CFLAGS) -DKK_IHEX_DISABLE_WRITING -c -o $@ $<

bin2ihex: bin2ihex.c kk_ihex_write.o
	$(CC) $(CFLAGS) $(LDFLAGS) -DKK_IHEX_DISABLE_READING -o $@ $+

ihex2bin: ihex2bin.c kk_ihex_read.o
	$(CC) $(CFLAGS) $(LDFLAGS) -DKK_IHEX_DISABLE_WRITING -o $@ $+


clean:
	rm -f $(OBJS) $(BINS)
