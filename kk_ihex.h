/*
 * kk_ihex.h: A simple library to read and write Intel HEX data. Intended
 * mainly for embedded systems.
 *
 *  Usage
 *
 * In order to actually write out data, you must provide an implementation
 * of the function:
 *      void ihex_flush_buffer(char *buffer, char *eptr);
 *
 * The sequence to write data in IHEX format is then:
 *
 *      struct ihex_state ihex;
 *      ihex_init(&ihex);
 *      ihex_write_at_address(&ihex, 0);
 *      ihex_write_bytes(&ihex, my_data, length_of_my_data);
 *      ihex_end_write(&ihex);
 *
 * These functions will then call `ihex_flush_buffer` as necessary - in
 * practice each line will be buffered internally. Note that the line buffer
 * may be overwritten immediately after `ihex_flush_buffer` returns;
 * it must be copied if it needs to be preserved.
 *
 *
 * Copyright (c) 2013 Kimmo Kulovesi, http://arkku.com/
 * Provided with absolutely no warranty, use at your own risk only.
 * Use and distribute freely, mark modified copies as such.
 */

#ifndef KK_IHEX_H
#define KK_IHEX_H

#include <stdint.h>

typedef uint_least32_t ihex_address_t;
typedef uint_least16_t ihex_segment_t;

// Maximum number of data bytes per line (applies to both reading and writing!)
#define IHEX_LINE_LENGTH 32

struct ihex_state {
    ihex_address_t address;
    ihex_segment_t segment;
    uint8_t flags;
    uint8_t length;
    uint8_t data[IHEX_LINE_LENGTH];
};

#define IHEX_FLAG_ADDRESS_OVERFLOW  1

enum ihex_record_type {
    IHEX_DATA_RECORD,
    IHEX_END_OF_FILE_RECORD,
    IHEX_EXTENDED_SEGMENT_ADDRESS_RECORD,
    IHEX_START_SEGMENT_ADDRESS_RECORD,
    IHEX_EXTENDED_LINEAR_ADDRESS_RECORD,
    IHEX_START_LINEAR_ADDRESS_RECORD
};

void ihex_init(struct ihex_state * const ihex);

// Begin writing at the given 32-bit `address`
// (can also be used to skip to a new address without calling `ihex_end_write`)
void ihex_write_at_address(struct ihex_state *ihex, ihex_address_t address);

// Write a single byte
void ihex_write_byte(struct ihex_state *ihex, uint8_t b);

// Write `count` bytes from `data`
void ihex_write_bytes(struct ihex_state *ihex, uint8_t *data, unsigned int count);

// End writing (flush buffers, write end of file record)
void ihex_end_write(struct ihex_state *ihex);


// Implement this to write out (eptr - buffer) bytes from buffer:
void ihex_flush_buffer(char *buffer, char *eptr);

/* Example implementation of ihex_flush_buffer:
 *
 *      void ihex_flush_buffer(char *buffer, char *eptr) {
 *          *eptr = '\0';
 *          (void) fputs(buffer, stdout);
 *      }
 */

// Resolve segmented address (if any), return the linear address
ihex_address_t ihex_linear_address(struct ihex_state *ihex);

#endif
