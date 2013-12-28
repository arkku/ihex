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

#define ADDRESS_HIGH_MASK 0xFFFF0000U

enum ihex_read_state {
    READ_WAIT_FOR_START = 1,
    READ_COUNT_HIGH,
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
    ihex->flags = (READ_WAIT_FOR_START << IHEX_READ_STATE_OFFSET);
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
ihex_end_read (struct ihex_state *ihex) {
    enum ihex_record_type type = ihex->flags & IHEX_READ_RECORD_TYPE_MASK;
    unsigned int len = ihex->length;
    if (len == 0 && type == IHEX_DATA_RECORD) {
        return;
    }
    {
        // compute checksum
        const uint8_t *r = ihex->data;
        unsigned int sum = len + type + (ihex->address & 0x00FFU) +
                           ((ihex->address & 0xFF00U) >> 8);
        while (len--) {
            sum += *r++;
        }
        len = ihex->length;
        ihex->data[len] ^= 0x100U - (sum & 0xFF);
    }
    if (ihex_data_read(ihex, type, ihex->data[len] != 0)) {
        if (type == IHEX_EXTENDED_LINEAR_ADDRESS_RECORD) {
            ihex_address_t addr = ihex->address & 0xFFFFU;
            addr |= ((ihex_address_t) ihex->data[0]) << 24;
            addr |= ((ihex_address_t) ihex->data[1]) << 16;
            ihex->address = addr;
#ifndef IHEX_DISABLE_SEGMENTS
        } else if (type == IHEX_EXTENDED_LINEAR_ADDRESS_RECORD) {
            ihex->segment = ihex->data[0] << 8 | ihex->data[1];
#endif
        }
    }
    ihex->length = 0;
    ihex->flags = 0;
}

void
ihex_read_byte (struct ihex_state *ihex, char byte) {
    enum ihex_read_state state = (ihex->flags & IHEX_READ_STATE_MASK);
    ihex->flags ^= state; // turn off the old state
    state >>= IHEX_READ_STATE_OFFSET;
    if (state && state != READ_WAIT_FOR_START) {
        int n;
        if (byte >= '0' && byte <= '9') {
            n = byte - '0';
        } else if (byte >= 'A' && byte <= 'F') {
            n = byte - ('A' - 10);
        } else if (byte >= 'a' && byte <= 'f') {
            n = byte - ('a' - 10);
        } else if (byte == '\n' || byte == '\r' || byte == '\0') {
            ihex_end_read(ihex);
            return;
        } else {
            goto save_read_state;
        }
        if ((state & 1) == (READ_COUNT_HIGH & 1)) {
            n <<= 4;
        }
        switch (state) {
        case READ_COUNT_HIGH:
            ihex->line_length = n;
            break;
        case READ_COUNT_LOW:
            n = ihex->line_length | n;
            ihex->line_length = n;
            if (n > IHEX_LINE_MAX_LENGTH) {
                ihex_end_read(ihex);
                return;
            }
            ihex->data[n] = 0xFF; // poison checksum
            break;
        case READ_ADDRESS_MSB_HIGH:
            ihex->address = (ihex->address & ADDRESS_HIGH_MASK) | (n << 8);
            break;
        case READ_ADDRESS_MSB_LOW:
            ihex->address |= n << 8;
            break;
        case READ_ADDRESS_LSB_HIGH:
        case READ_ADDRESS_LSB_LOW:
            ihex->address |= n;
            break;
        case READ_RECORD_TYPE_HIGH:
            ihex->flags = (ihex->flags & ~IHEX_READ_RECORD_TYPE_MASK);
        case READ_RECORD_TYPE_LOW:
            if (n > IHEX_READ_RECORD_TYPE_MASK) {
                // unknown record type
                state = READ_WAIT_FOR_START;
                goto save_read_state;
            }
            ihex->flags |= (n & IHEX_READ_RECORD_TYPE_MASK);
            break;
        case READ_DATA_HIGH:
            ihex->data[ihex->length] = n;
            break;
        case READ_DATA_LOW: {
            unsigned int len = ihex->length;
            ihex->data[len] |= n;
            if (len == ihex->line_length) {
                ihex_end_read(ihex);
                state = READ_WAIT_FOR_START;
            } else {
                ihex->length = len + 1;
                state = READ_DATA_HIGH;
            }
            goto save_read_state;
        }
        default:
            state = READ_WAIT_FOR_START;
            goto save_read_state;
        }
        ++state;
    } else if (byte == IHEX_START) {
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

