/*
 * merge32bit.c: Merge four 8-bit files into a single 32-bit file.
 *
 * The command-line option `-o` specifies the output file, and the options
 * `-0`, `-1`, `-2`, and `-3` specify the input files. The output file is
 * created by alternately writing one byte from each input file. This can
 * be used to merge a 32-bit ROM image that has been split into four 8-bit
 * images into a a single 32-bit file.
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
    FILE *outfile = stdout;
    FILE *infile[4] = { NULL };
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
            case '3':
            case '2':
            case '1':
            case '0': {
                int byte_number = arg[1] - '0';
                if (--argc == 0) {
                    goto invalid_argument;
                }
                ++argv;
                if (!(infile[byte_number] = fopen(*argv, "rb"))) {
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
        (void) fprintf(stderr, "merge32bit - Copyright (c) 2019 Kimmo Kulovesi\n");
        (void) fprintf(stderr, "Usage: merge32bit [-o <out.bin>] <-{0,1,2,3} inN.bin>\n");
        return arg ? EXIT_FAILURE : EXIT_SUCCESS;
argument_error:
        perror(*argv);
        return EXIT_FAILURE;
    }

    if (!(infile[0] && infile[1] && infile[2] && infile[3])) {
        arg = "";
        goto usage;
    }

    errno = 0;
    for (;;) {
        for (i = 0; i < 4; ++i) {
            int byte = fgetc(infile[i]);
            if (byte == EOF) {
                goto end_read;
            }
            if (fputc(byte, outfile) == EOF) {
                goto end_read;
            }
        }
    }
end_read:

    if (errno) {
        perror("Error");
    }

    for (i = 0; i < 4; ++i) {
        (void) fclose(infile[i]);
    }
    if (outfile != stdin) {
        (void) fclose(outfile);
    }

    return errno ? EXIT_FAILURE : EXIT_SUCCESS;
}
