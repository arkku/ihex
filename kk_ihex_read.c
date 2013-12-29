/*
 * kk_ihex_read.c: A simple library for reading the Intel HEX (IHEX) format.
 *
 * See the header `kk_ihex.h` for instructions.
 *
 * Copyright (c) 2013 Kimmo Kulovesi, http://arkku.com/
 * Provided with absolutely no warranty, use at your own risk only.
 * Use and distribute freely, mark modified copies as such.
 */

#include "kk_ihex_read.h"

#define IHEX_START ':'

#define ADDRESS_HIGH_MASK ((ihex_address_t) 0xFFFF0000U)

enum ihex_read_state {
    READ_WAIT_FOR_START = 0,
    READ_COUNT_HIGH = 1,
    READ_COUNT_LOW,
    READ_ADDRESS_MSB_HIGH,
    READ_ADDRESS_MSB_LOW,
    READ_ADDRESS_LSB_HIGH,
    READ_ADDRESS_LSB_LOW,
    READ_RECORD_TYPE_HIGH,
    READ_RECORD_TYPE_LOW,
    READ_DATA_HIGH,
    READ_DATA_LOW
};

#define IHEX_READ_RECORD_TYPE_MASK 0x07
#define IHEX_READ_STATE_MASK 0x78
#define IHEX_READ_STATE_OFFSET 3

void
ihex_begin_read (struct ihex_state *ihex) {
    ihex->address = 0;
#ifndef IHEX_DISABLE_SEGMENTS
    ihex->segment = 0;
#endif
    ihex->flags = 0;
    ihex->line_length = 0;
    ihex->length = 0;
}

void
ihex_read_at_address (struct ihex_state *ihex, ihex_address_t address) {
    ihex_begin_read(ihex);
    ihex->address = address;
}

#ifndef IHEX_DISABLE_SEGMENTS
void
ihex_read_at_segment (struct ihex_state *ihex, ihex_segment_t segment) {
    ihex_begin_read(ihex);
    ihex->segment = segment;
}
#endif

void
ihex_end_read (struct ihex_state * const ihex) {
    enum ihex_record_type type = ihex->flags & IHEX_READ_RECORD_TYPE_MASK;
    unsigned int len = ihex->length;
    uint8_t * const eptr = ihex->data + len; // checksum stored at the end
    if (len == 0 && type == IHEX_DATA_RECORD) {
        return;
    }
    {
        // compute checksum
        const uint8_t *r = ihex->data;
        unsigned int sum = len + type + (ihex->address & 0x00FFU) +
                           ((ihex->address & 0xFF00U) >> 8);
        while (r != eptr) {
            sum += *r++;
        }
        *eptr ^= ~sum + 1U;
    }
    if (ihex_data_read(ihex, type, *eptr)) {
        if (type == IHEX_EXTENDED_LINEAR_ADDRESS_RECORD) {
            ihex->address &= 0xFFFFU;
            ihex->address |= ((ihex_address_t) ihex->data[0]) << 24;
            ihex->address |= ((ihex_address_t) ihex->data[1]) << 16;
#ifndef IHEX_DISABLE_SEGMENTS
        } else if (type == IHEX_EXTENDED_SEGMENT_ADDRESS_RECORD) {
            ihex->segment = (ihex->data[0] << 8) | ihex->data[1];
#endif
        }
    }
    ihex->length = 0;
    ihex->flags = 0;
}

void
ihex_read_byte (struct ihex_state *ihex, int b) {
    enum ihex_read_state state = (ihex->flags & IHEX_READ_STATE_MASK);
    ihex->flags ^= state; // turn off the old state
    state >>= IHEX_READ_STATE_OFFSET;
    if (b != IHEX_START) {
        if (b >= '0' && b <= '9') {
            b -= '0';
        } else if (b >= 'A' && b <= 'F') {
            b -= 'A' - 10;
        } else if (b >= 'a' && b <= 'f') {
            b -= 'a' - 10;
        } else {
            // ignore extra characters
            goto save_read_state;
        }
        {
            unsigned int len = ihex->length;
            if (state & 1) {
                // high nybble, store temporarily at end of data:
                ihex->data[len] = b << 4;
            } else {
                // low nybble, combine with stored high nybble:
                b = (ihex->data[len] |= b);
                switch (state >> 1) {
                case (READ_COUNT_LOW >> 1):
                    // data length
                    ihex->line_length = b;
                    if (b > IHEX_LINE_MAX_LENGTH) {
                        ihex_end_read(ihex);
                        return;
                    }
                    break;
                case (READ_ADDRESS_MSB_LOW >> 1):
                    // high byte of 16-bit address
                    ihex->address = (ihex->address & ADDRESS_HIGH_MASK);
                    b <<= 8;
                case (READ_ADDRESS_LSB_LOW >> 1):
                    // low byte of 16-bit address
                    ihex->address |= b;
                    break;
                case (READ_RECORD_TYPE_LOW >> 1):
                    // record type
                    if (b > IHEX_READ_RECORD_TYPE_MASK) {
                        // skip non-standard record types silently
                        return;
                    } 
                    ihex->flags = (ihex->flags & ~IHEX_READ_RECORD_TYPE_MASK) | b;
                    break;
                case (READ_DATA_LOW >> 1):
                    if (len < ihex->line_length) {
                        // data byte
                        ihex->length = len + 1;
                        --state; // back to READ_DATA_HIGH
                        goto save_read_state;
                    }
                    // end of line (last "data" byte is checksum)
                    ihex_end_read(ihex);
                default:
                    return;
                }
            }
            ++state;
        }
    } else {
        // start of record
#if IHEX_CATCH_TRUNCATED_LINES != 0
        ihex_end_read(ihex);
#endif
        state = READ_COUNT_HIGH;
    }
save_read_state:
    ihex->flags |= state << IHEX_READ_STATE_OFFSET;
}

void
ihex_read_bytes (struct ihex_state *ihex, char *data, unsigned int count) {
    while (count--) {
        ihex_read_byte(ihex, *data++);
    }
}

