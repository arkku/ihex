/*
 * bin2ihex.c: Read binary data from stdin, output in IHEX format.
 *
 * By Kimmo Kulovesi, http://arkku.com
 */

#include "kk_ihex.h"
#include <stdio.h>
#include <stdlib.h>

int
main (void) {
    int c;
    struct ihex_state ihex;

    ihex_init(&ihex);
    ihex_write_at_address(&ihex, 0);
    while ((c = fgetc(stdin)) != EOF) {
        ihex_write_byte(&ihex, c);
    }
    ihex_end_write(&ihex);

    return EXIT_SUCCESS;
}

void ihex_flush_buffer(char *buffer, char *eptr) {
    *eptr = '\0';
    (void) fputs(buffer, stdout);
}
