/*
 * ihex2bin.c: Read Intel HEX format, write binary data.
 *
 * By default reads from stdin and writes to stdout. The command-line
 * options `-i` and `-o` can be used to specify the input and output
 * file, respectively. Specifying an output file allows sparse writes.
 *
 * NOTE: Many Intel HEX files produced by compilers/etc have data
 * beginning at an address greater than zero, potentially causing very
 * unnecessarily large files to be created. The command-line option
 * `-a` can be used to specify the start address of the output file,
 * i.e., the value will be subtracted from the IHEX addresses (the
 * result must not be negative).
 *
 * Alternatively, the command-line option `-A` sets the address offset
 * to the first address that would be written (i.e., first byte of
 * data written will be at address 0).
 *
 * Copyright (c) 2013-2019 Kimmo Kulovesi, https://arkku.com
 * Provided with absolutely no warranty, use at your own risk only.
 * Distribute freely, mark modified copies as such.
 */

#include "kk_ihex_read.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define AUTODETECT_ADDRESS (~0UL)

static FILE *outfile;
static unsigned long line_number = 1L;
static unsigned long file_position = 0L;
static unsigned long address_offset = 0UL;
static bool debug_enabled = 0;

int
main (int argc, char *argv[]) {
    struct ihex_state ihex;
    FILE *infile = stdin;
    ihex_count_t count;
    char buf[256];

    outfile = stdout;

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
                address_offset = strtoul(*argv, &arg, 0);
                if (errno || arg == *argv) {
                    errno = errno ? errno : EINVAL;
                    goto argument_error;
                }
                break;
            case 'A':
                address_offset = AUTODETECT_ADDRESS;
                break;
            case 'i':
                if (--argc == 0) {
                    goto invalid_argument;
                }
                ++argv;
                if (!(infile = fopen(*argv, "r"))) {
                    goto argument_error;
                }
                break;
            case 'o':
                if (--argc == 0) {
                    goto invalid_argument;
                }
                ++argv;
                if (!(outfile = fopen(*argv, "wb"))) {
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
                               " - Copyright (c) 2013-2015 Kimmo Kulovesi\n");
        (void) fprintf(stderr, "Usage: ihex2bin ([-a <address_offset>]|[-A])"
                                " [-o <out.bin>] [-i <in.hex>] [-v]\n");
        return arg ? EXIT_FAILURE : EXIT_SUCCESS;
argument_error:
        perror(*argv);
        return EXIT_FAILURE;
    }

    ihex_read_at_address(&ihex, (address_offset != AUTODETECT_ADDRESS) ?
                                (ihex_address_t) address_offset :
                                0);
    while (fgets(buf, sizeof(buf), infile)) {
        count = (ihex_count_t) strlen(buf);
        ihex_read_bytes(&ihex, buf, count);
        line_number += (count && buf[count - 1] == '\n');
    }
    ihex_end_read(&ihex);

    if (infile != stdin) {
        (void) fclose(infile);
    }

    return EXIT_SUCCESS;
}

ihex_bool_t
ihex_data_read (struct ihex_state *ihex,
                ihex_record_type_t type,
                ihex_bool_t error) {
    if (error) {
        (void) fprintf(stderr, "Checksum error on line %lu\n", line_number);
        exit(EXIT_FAILURE);
    }
    if ((error = (ihex->length < ihex->line_length))) {
        (void) fprintf(stderr, "Line length error on line %lu\n", line_number);
        exit(EXIT_FAILURE);
    }
    if (!outfile) {
        (void) fprintf(stderr, "Excess data after end of file record\n");
        exit(EXIT_FAILURE);
    }
    if (type == IHEX_DATA_RECORD) {
        unsigned long address = (unsigned long) IHEX_LINEAR_ADDRESS(ihex);
        if (address < address_offset) {
            if (address_offset == AUTODETECT_ADDRESS) {
                // autodetect initial address
                address_offset = address;
                if (debug_enabled) {
                    (void) fprintf(stderr, "Address offset: 0x%lx\n",
                            address_offset);
                }
            } else {
                (void) fprintf(stderr, "Address underflow on line %lu\n",
                        line_number);
                exit(EXIT_FAILURE);
            }
        }
        address -= address_offset;
        if (address != file_position) {
            if (debug_enabled) {
                (void) fprintf(stderr,
                        "Seeking from 0x%lx to 0x%lx on line %lu\n",
                        file_position, address, line_number);
            }
            if (outfile == stdout || fseek(outfile, (long) address, SEEK_SET)) {
                if (file_position < address) {
                    // "seek" forward in stdout by writing NUL bytes
                    do {
                        (void) fputc('\0', outfile);
                    } while (++file_position < address);
                } else {
                    perror("fseek");
                    exit(EXIT_FAILURE);
                }
            }
            file_position = address;
        }
        if (!fwrite(ihex->data, ihex->length, 1, outfile)) {
            perror("fwrite");
            exit(EXIT_FAILURE);
        }
        file_position += ihex->length;
    } else if (type == IHEX_END_OF_FILE_RECORD) {
        if (debug_enabled) {
            (void) fprintf(stderr, "%lu bytes written\n", file_position);
        }
        if (outfile != stdout) {
            (void) fclose(outfile);
        }
        outfile = NULL;
    }
    return true;
}
