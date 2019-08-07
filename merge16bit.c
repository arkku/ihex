/*
 * merge16bit.c: Merge a 16-bit ROM binary from two 8-bit images.
 *
 * The command-line options `-h` and `-l` specify the high and low
 * input files, respectively. The inputs should be raw 8-bit binary
 * ROM image halves (raw data, split into two files). The option
 * `-o` specifies the output file, which will be a 16-bit ROM image.
 * Output is to `stdout` by default.
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
    FILE *outfile = stdout;
    FILE *inhigh = NULL;
    FILE *inlow = NULL;
    char *arg = NULL;

    while (--argc) {
        arg = *(++argv);
        if (arg[0] == '-' && arg[1] && arg[2] == '\0') {
            switch (arg[1]) {
            case 'o':
                if (--argc == 0) {
                    goto invalid_argument;
                }
                ++argv;
                if (!(outfile = fopen(*argv, "wb"))) {
                    goto argument_error;
                }
                break;
            case 'h':
                if (--argc == 0) {
                    goto invalid_argument;
                }
                ++argv;
                if (!(inhigh = fopen(*argv, "rb"))) {
                    goto argument_error;
                }
                break;
            case 'l':
                if (--argc == 0) {
                    goto invalid_argument;
                }
                ++argv;
                if (!(inlow = fopen(*argv, "rb"))) {
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
        (void) fprintf(stderr, "merge16bit - Copyright (c) 2019 Kimmo Kulovesi\n");
        (void) fprintf(stderr, "Usage: merge16bit [-o <out.bin>] <-h highfile> <-l lowfile>\n");
        return arg ? EXIT_FAILURE : EXIT_SUCCESS;
argument_error:
        perror(*argv);
        return EXIT_FAILURE;
    }

    if (!(inhigh && inlow)) {
        arg = "";
        goto usage;
    }

    errno = 0;
    for (;;) {
        int byte = fgetc(inlow);
        if (byte == EOF) {
            break;
        }
        if (fputc(byte, outfile) == EOF) {
            break;
        }
        byte = fgetc(inhigh);
        if (byte == EOF) {
            break;
        }
        if (fputc(byte, outfile) == EOF) {
            break;
        }
    }

    if (errno) {
        perror("Error");
    }

    (void) fclose(inlow);
    (void) fclose(inhigh);
    if (outfile != stdout) {
        (void) fclose(outfile);
    }

    return errno ? EXIT_FAILURE : EXIT_SUCCESS;
}
