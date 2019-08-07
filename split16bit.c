/*
 * split16bit.c: Split a 16-bit ROM binary into two 8-bit images.
 *
 * The command-line option `-i` specifies the input file, which should
 * be a 16-bit binary ROM image (raw data), and the options `-h` and
 * `-l` specify the high and low output files, respectively. Input is
 * read from `stdin` by default.
 *
 * Copyright (c) 2019 Kimmo Kulovesi, https://arkku.com
 * Provided with absolutely no warranty, use at your own risk only.
 * Distribute freely, mark modified copies as such.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int
main (int argc, char *argv[]) {
    FILE *infile = stdin;
    FILE *outhigh = NULL;
    FILE *outlow = NULL;
    char *arg = NULL;

    while (--argc) {
        arg = *(++argv);
        if (arg[0] == '-' && arg[1] && arg[2] == '\0') {
            switch (arg[1]) {
            case 'i':
                if (--argc == 0) {
                    goto invalid_argument;
                }
                ++argv;
                if (!(infile = fopen(*argv, "r"))) {
                    goto argument_error;
                }
                break;
            case 'h':
                if (--argc == 0) {
                    goto invalid_argument;
                }
                ++argv;
                if (!(outhigh = fopen(*argv, "wb"))) {
                    goto argument_error;
                }
                break;
            case 'l':
                if (--argc == 0) {
                    goto invalid_argument;
                }
                ++argv;
                if (!(outlow = fopen(*argv, "wb"))) {
                    goto argument_error;
                }
                break;
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
        (void) fprintf(stderr, "split16bit - Copyright (c) 2019 Kimmo Kulovesi\n");
        (void) fprintf(stderr, "Usage: split16bit [-i <in.bin>] <-h highfile> <-l lowfile>\n");
        return arg ? EXIT_FAILURE : EXIT_SUCCESS;
argument_error:
        perror(*argv);
        return EXIT_FAILURE;
    }

    if (!(outhigh && outlow)) {
        arg = "";
        goto usage;
    }

    errno = 0;
    for (;;) {
        int byte = fgetc(infile);
        if (byte == EOF) {
            break;
        }
        if (fputc(byte, outlow) == EOF) {
            break;
        }
        byte = fgetc(infile);
        if (byte == EOF) {
            break;
        }
        if (fputc(byte, outhigh) == EOF) {
            break;
        }
    }

    if (errno) {
        perror("Error");
    }

    (void) fclose(outhigh);
    (void) fclose(outlow);
    if (infile != stdin) {
        (void) fclose(infile);
    }

    return errno ? EXIT_FAILURE : EXIT_SUCCESS;
}
