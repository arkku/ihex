/*
 * bin2ihex.c: Read binary data, output in Intel HEX format.
 *
 * By default reads from stdin and writes to stdout. Input and
 * output files can be specified with arguments `-i` and `-o`,
 * respectively. Initial address offset can be set with option
 * `-a` (also, `-a 0` forces output of the initial offset even
 * though it is the default zero). The number of bytes to encode
 * into a single line of output (which will be more than twice
 * that length in bytes) can be given with the argument `-b`.
 *
 * Copyright (c) 2013-2019 Kimmo Kulovesi, https://arkku.com
 * Provided with absolutely no warranty, use at your own risk only.
 * Distribute freely, mark modified copies as such.
 */

#include "kk_ihex_write.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#ifdef IHEX_EXTERNAL_WRITE_BUFFER
char *ihex_write_buffer = NULL;
#endif

//#define IHEX_WRITE_INITIAL_EXTENDED_ADDRESS_RECORD

static FILE *outfile;

int
main (int argc, char *argv[]) {
    struct ihex_state ihex;
    FILE *infile = stdin;
    ihex_address_t initial_address = 0;
    uint8_t line_length = IHEX_DEFAULT_OUTPUT_LINE_LENGTH;
    bool write_initial_address = 0;
    bool debug_enabled = 0;
    ihex_count_t count;
    uint8_t buf[1024];

    outfile = stdout;

    // spaghetti parser of args: -o outfile -i infile -a initial_address
    while (--argc) {
        char *arg = *(++argv);
        if (arg[0] == '-' && arg[1] && arg[2] == '\0') {
            switch (arg[1]) {
            case 'a':
                if (--argc == 0) {
                    goto invalid_argument;
                }
                ++argv;
                errno = 0;
                initial_address = (ihex_address_t) strtoul(*argv, &arg, 0);
                if (errno || arg == *argv) {
                    errno = errno ? errno : EINVAL;
                    goto argument_error;
                }
                write_initial_address = 1;
                break;
            case 'i':
                if (--argc == 0) {
                    goto invalid_argument;
                }
                ++argv;
                if (!(infile = fopen(*argv, "rb"))) {
                    goto argument_error;
                }
                break;
            case 'b':
                if (--argc == 0) {
                    goto invalid_argument;
                }
                ++argv;
                errno = 0;
                line_length = (uint8_t) strtoul(*argv, &arg, 0);
                if (errno || arg == *argv || !line_length || line_length > IHEX_MAX_OUTPUT_LINE_LENGTH) {
                    errno = errno ? errno : EINVAL;
                    goto argument_error;
                }
                break;
            case 'o':
                if (--argc == 0) {
                    goto invalid_argument;
                }
                ++argv;
                if (!(outfile = fopen(*argv, "w"))) {
                    goto argument_error;
                }
                break;
            case 'v':
                debug_enabled = 1;
                break;
            case 'h':
            case '?':
                arg = NULL;
                goto usage;
            default:
                goto invalid_argument;
            }
            continue;
        }
invalid_argument:
        (void) fprintf(stderr, "Invalid argument: %s\n", arg);
usage:
        (void) fprintf(stderr, "kk_ihex " KK_IHEX_VERSION
                               " - Copyright (c) 2013-2019 Kimmo Kulovesi\n");
        (void) fprintf(stderr, "Usage: bin2ihex [-a <address_offset>]"
                               " [-o <out.hex>] [-i <in.bin>] [-b <length>] [-v]\n");
        return arg ? EXIT_FAILURE : EXIT_SUCCESS;
argument_error:
        perror(*argv);
        return EXIT_FAILURE;
    }

    {
#ifdef IHEX_EXTERNAL_WRITE_BUFFER
        // How to provide an external write buffer with limited duration:
        char buffer[IHEX_WRITE_BUFFER_LENGTH];
        ihex_write_buffer = buffer;
#endif
        ihex_init(&ihex);
        ihex_set_output_line_length(&ihex, line_length);
        ihex_write_at_address(&ihex, initial_address);
        if (write_initial_address) {
            if (debug_enabled) {
                (void) fprintf(stderr, "Address offset: 0x%lx\n",
                        (unsigned long) ihex.address);
            }
            ihex.flags |= IHEX_FLAG_ADDRESS_OVERFLOW;
        }
        while ((count = (ihex_count_t) fread(buf, 1, sizeof(buf), infile))) {
            ihex_write_bytes(&ihex, buf, count);
        }
        ihex_end_write(&ihex);
#ifdef IHEX_EXTERNAL_WRITE_BUFFER
        ihex_write_buffer = NULL;
#endif
    }

    if (outfile != stdout) {
        (void) fclose(outfile);
    }
    if (infile != stdout) {
        (void) fclose(infile);
    }

    if (debug_enabled) {
        (void) fprintf(stderr, "%lu bytes read\n",
                (unsigned long) ihex.address - initial_address);
    }

    return EXIT_SUCCESS;
}

#pragma clang diagnostic ignored "-Wunused-parameter"

void
ihex_flush_buffer(struct ihex_state *ihex, char *buffer, char *eptr) {
    *eptr = '\0';
    (void) fputs(buffer, outfile);
}
