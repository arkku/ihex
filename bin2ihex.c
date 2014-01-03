/*
 * bin2ihex.c: Read binary data, output in Intel HEX format.
 *
 * By default reads from stdin and writes to stdout. Input and
 * output files can be specified with arguments `-i` and `-o`,
 * respectively. Initial address offset can be set with option
 * `-a` (also, `-a 0` forces output of the initial offset even
 * though it is the default zero).
 *
 * Copyright (c) 2013 Kimmo Kulovesi, http://arkku.com
 * Provided with absolutely no warranty, use at your own risk only.
 * Distribute freely, mark modified copies as such.
 */

#include "kk_ihex_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

//#define IHEX_WRITE_INITIAL_EXTENDED_ADDRESS_RECORD

static FILE *outfile;

int
main (int argc, char *argv[]) {
    struct ihex_state ihex;
    FILE *infile = stdin;
    ihex_address_t initial_address = 0;
    _Bool write_initial_address = 0;
    _Bool debug_enabled = 0;
    unsigned int count;
    uint8_t buf[1024];

    outfile = stdout;

    // spaghetti parser of args: -o outfile -i infile -a initial_address
    for (int i = 1; i < argc; ++i) {
        char *arg = argv[i];
        if (arg[0] == '-' && arg[1] && arg[2] == '\0') {
            switch (arg[1]) {
            case 'a':
                if (++i == argc) {
                    goto invalid_argument;
                }
                errno = 0;
                initial_address = (ihex_address_t) strtoul(argv[i], &arg, 0);
                if (errno || arg == argv[i]) {
                    errno = errno ? errno : EINVAL;
                    goto argument_error;
                }
                write_initial_address = 1;
                break;
            case 'o':
                if (++i == argc) {
                    goto invalid_argument;
                }
                if (!(outfile = fopen(argv[i], "w"))) {
                    goto argument_error;
                }
                break;
            case 'i':
                if (++i == argc) {
                    goto invalid_argument;
                }
                if (!(infile = fopen(argv[i], "rb"))) {
                    goto argument_error;
                }
                break;
            case 'v':
                debug_enabled = 1;
                break;
            case 'h':
            case '?':
                i = EXIT_SUCCESS;
                goto usage;
            default:
                goto invalid_argument;
            }
            continue;
        }
invalid_argument:
        (void) fprintf(stderr, "Invalid argument: %s\n", arg);
usage:
        (void) fprintf(stderr, "Usage: %s [-a <address_offset>]"
                               " [-o <out.hex>] [-i <in.bin>] [-v]\n",
                       argv[0]);
        return i;
argument_error:
        perror(argv[i]);
        return EXIT_FAILURE;
    }

    ihex_init(&ihex);
    ihex_write_at_address(&ihex, initial_address);
    if (write_initial_address) {
        if (debug_enabled) {
            (void) fprintf(stderr, "Address offset: 0x%lx\n",
                    (unsigned long) ihex.address);
        }
        ihex.flags |= IHEX_FLAG_ADDRESS_OVERFLOW;
    }
    while ((count = fread(buf, 1, sizeof(buf), infile))) {
        ihex_write_bytes(&ihex, buf, count);
    }
    ihex_end_write(&ihex);

    if (debug_enabled) {
        (void) fprintf(stderr, "%lu bytes read\n",
                (unsigned long) ihex.address);
    }

    return EXIT_SUCCESS;
}

#pragma clang diagnostic ignored "-Wunused-parameter"

void
ihex_flush_buffer(struct ihex_state *ihex, char *buffer, char *eptr) {
    *eptr = '\0';
    (void) fputs(buffer, outfile);
}
