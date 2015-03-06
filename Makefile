CC=clang
CFLAGS=-Wall -std=c99 -pedantic -Wextra -Weverything -Wno-padded -Os #-emit-llvm
LDFLAGS=-Os
AR=ar
ARFLAGS=rcs

OBJS = kk_ihex_write.o kk_ihex_read.o bin2ihex.o ihex2bin.o
BINPATH = ./
LIBPATH = ./
BINS = $(BINPATH)bin2ihex $(BINPATH)ihex2bin
LIB = $(LIBPATH)libkk_ihex.a
TESTFILE = $(LIB)
TESTER = 
#TESTER = valgrind

.PHONY: all clean distclean test

all: $(BINS) $(LIB)

$(OBJS): kk_ihex.h
$(BINS): | $(BINPATH)
$(LIB): | $(LIBPATH)
bin2ihex.o kk_ihex_write.o: kk_ihex_write.h
ihex2bin.o kk_ihex_read.o: kk_ihex_read.h

$(LIB): kk_ihex_write.o kk_ihex_read.o
	$(AR) $(ARFLAGS) $@ $+

$(BINPATH)bin2ihex: bin2ihex.o $(LIB)
	$(CC) $(LDFLAGS) -o $@ $+

$(BINPATH)ihex2bin: ihex2bin.o $(LIB)
	$(CC) $(LDFLAGS) -o $@ $+

$(sort $(BINPATH) $(LIBPATH)):
	@mkdir -p $@

test: $(BINPATH)bin2ihex $(BINPATH)ihex2bin $(TESTFILE)
	@$(TESTER) $(BINPATH)bin2ihex -v -a 0x80 -i '$(TESTFILE)' | \
	    $(TESTER) $(BINPATH)ihex2bin -A -v | \
	    diff '$(TESTFILE)' -
	@echo Loopback test success!

clean:
	rm -f $(OBJS)

distclean: | clean
	rm -f $(BINS) $(LIB)
	@rmdir $(BINPATH) $(LIBPATH) >/dev/null 2>/dev/null || true

