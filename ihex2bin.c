/*
 * ihex2bin.c: Read Intel HEX data from stdin, write binary data to stdout
 * or a file specified on the command line. Specifying output file allows
 * sparse and/or unordered data to be written.
 *
 * Copyright (c) 2013 Kimmo Kulovesi, http://arkku.com
 * Provided with absolutely no warranty, use at your own risk only.
 * Distribute freely, mark modified copies as such.
 */

#include "kk_ihex.h"
#include <stdio.h>
#include <stdlib.h>

static FILE *outfile;
static unsigned long line_number = 1;
static unsigned long file_position = 0L;

int
main (int argc, char *argv[]) {
    struct ihex_state ihex;
    int c;

    if (argc == 2) {
        if (!(outfile = fopen(argv[1], "wb"))) {
            perror(argv[1]);
            return EXIT_FAILURE;
        }
    } else {
        outfile = stdout;
    }
    ihex_init(&ihex);
    ihex_begin_read(&ihex);

    while ((c = fgetc(stdin)) != EOF) {
        ihex_read_byte(&ihex, c);
        line_number += (c == '\n') ? 1 : 0;
    }
    ihex_end_read(&ihex);

    return EXIT_SUCCESS;
}

bool ihex_data_read (struct ihex_state *ihex,
                     enum ihex_record_type type,
                     bool error) {
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
        unsigned long address = (unsigned long) ihex_linear_address(ihex);
        if (address != file_position) {
            (void) fprintf(stderr, "Seeking from %lu to %lu on line %lu\n",
                           file_position, address, line_number);
            if (file_position < address && outfile == stdout) {
                // "seek" forward in stdout by writing NUL bytes
                do {
                    (void) fputc('\0', outfile);
                } while (++file_position < address);
            } else if (fseek(outfile, address, SEEK_SET)) {
                perror("fseek");
                exit(EXIT_FAILURE);
            }
            file_position = address;
        }
        if (!fwrite(ihex->data, ihex->length, 1, outfile)) {
            perror("fwrite");
            exit(EXIT_FAILURE);
        }
        file_position += ihex->length;
    } else if (type == IHEX_END_OF_FILE_RECORD) {
        (void) fprintf(stderr, "%lu bytes written\n", file_position);
        if (outfile != stdout) {
            (void) fclose(outfile);
        }
        outfile = NULL;
    }
    return true;
}
