/*
 * split32bit.c: Split a 32-bit ROM binary into four 8-bit images.
 *
 * The command-line option `-i` specifies the input file, which should
 * be a 32-bit binary ROM image (raw data), and the options `-0`, `-1`,
 * `-2`, and `-3` specify the output files for each of the four bytes
 * that make up the 32-bit dword.
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
    int i;
    FILE *infile = stdin;
    FILE *outfile[4] = { NULL };
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
            case '3':
            case '2':
            case '1':
            case '0': {
                int byte_number = arg[1] - '0';
                if (--argc == 0) {
                    goto invalid_argument;
                }
                ++argv;
                if (!(outfile[byte_number] = fopen(*argv, "wb"))) {
                    goto argument_error;
                }
                break;
            }
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
        (void) fprintf(stderr, "split32bit - Copyright (c) 2019 Kimmo Kulovesi\n");
        (void) fprintf(stderr, "Usage: split32bit [-i <in.bin>] <-{0,1,2,3} outN.bin>\n");
        return arg ? EXIT_FAILURE : EXIT_SUCCESS;
argument_error:
        perror(*argv);
        return EXIT_FAILURE;
    }

    if (!(outfile[0] && outfile[1] && outfile[2] && outfile[3])) {
        arg = "";
        goto usage;
    }

    errno = 0;
    for (;;) {
        for (i = 0; i < 4; ++i) {
            int byte = fgetc(infile);
            if (byte == EOF) {
                goto end_read;
            }
            if (fputc(byte, outfile[i]) == EOF) {
                goto end_read;
            }
        }
    }
end_read:

    if (errno) {
        perror("Error");
    }

    for (i = 0; i < 4; ++i) {
        (void) fclose(outfile[i]);
    }
    if (infile != stdin) {
        (void) fclose(infile);
    }

    return errno ? EXIT_FAILURE : EXIT_SUCCESS;
}
