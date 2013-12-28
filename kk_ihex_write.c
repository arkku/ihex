/*
 * kk_ihex_write.c: A simple library for writing the Intel HEX (IHEX) format.
 *
 * See the header `kk_ihex.h` for instructions.
 *
 * Copyright (c) 2013 Kimmo Kulovesi, http://arkku.com/
 * Provided with absolutely no warranty, use at your own risk only.
 * Use and distribute freely, mark modified copies as such.
 */

#include "kk_ihex_write.h"

#define IHEX_START ':'

static const char IHEX_NEWLINE[] = IHEX_NEWLINE_STRING;

#define ADDRESS_HIGH_MASK ((ihex_address_t) 0xFFFF0000U)
#define ADDRESS_HIGH_BYTES(addr) ((addr) >> 16)

#define HEX_DIGIT(n) ((n) + ( ((n) < 10) ? '0' : ('A' - 10)))

static char line_buffer[1+2+4+2+(IHEX_MAX_OUTPUT_LINE_LENGTH*2)+2+sizeof(IHEX_NEWLINE)];

#if IHEX_MAX_OUTPUT_LINE_LENGTH > IHEX_LINE_MAX_LENGTH
#error "IHEX_MAX_OUTPUT_LINE_LENGTH > IHEX_LINE_MAX_LENGTH"
#endif

void
ihex_init (struct ihex_state * const ihex) {
    ihex->address = 0;
#ifndef IHEX_DISABLE_SEGMENTS
    ihex->segment = 0;
#endif
    ihex->flags = 0;
    ihex->line_length = IHEX_DEFAULT_OUTPUT_LINE_LENGTH;
    ihex->length = 0;
}

static char *
ihex_buffer_byte (char *w, const unsigned int byte) {
    unsigned int n = (byte & 0xF0U) >> 4; // high nybble
    *w++ = HEX_DIGIT(n);
    n = byte & 0x0FU; // low nybble
    *w++ = HEX_DIGIT(n);
    return w;
}

static char *
ihex_buffer_word (char *w, const unsigned int word, unsigned int * const checksum) {
    unsigned int byte = (word & 0xFF00U) >> 8; // high byte
    w = ihex_buffer_byte(w, byte);
    *checksum += byte;
    byte = word & 0x00FFU; // low byte
    *checksum += byte;
    return ihex_buffer_byte(w, byte);
}

static char *
ihex_buffer_newline (char *w) {
    const char *r = IHEX_NEWLINE;
    do {
        *w++ = *r++;
    } while (*r);
    return w;
}

static void
ihex_write_end_of_file (struct ihex_state * const ihex) {
    char *w = line_buffer;
    *w++ = IHEX_START;          // :
#if 0
    for (unsigned int i = 7; i; --i) {
        *w++ = '0';
    }
    *w++ = '1';
    *w++ = 'F';
    *w++ = 'F';
#else
    w = ihex_buffer_byte(w, 0); // length
    w = ihex_buffer_byte(w, 0); // address msb
    w = ihex_buffer_byte(w, 0); // address lsb
    w = ihex_buffer_byte(w, IHEX_END_OF_FILE_RECORD); // record type
    w = ihex_buffer_byte(w, ~IHEX_END_OF_FILE_RECORD + 1); // checksum
#endif
    w = ihex_buffer_newline(w);
    ihex_flush_buffer(ihex, line_buffer, w);
}

static void
ihex_write_extended_address (struct ihex_state * const ihex,
                             const ihex_segment_t address,
                             const enum ihex_record_type type) {
    char *w = line_buffer;
    unsigned int sum = type + 2;

    *w++ = IHEX_START;          // :
    w = ihex_buffer_byte(w, 2); // length
    w = ihex_buffer_byte(w, 0); // 16-bit address msb
    w = ihex_buffer_byte(w, 0); // 16-bit address lsb
    w = ihex_buffer_byte(w, type); // record type
    w = ihex_buffer_word(w, address, &sum); // high bytes of address
    w = ihex_buffer_byte(w, ~sum + 1); // checksum
    w = ihex_buffer_newline(w);
    ihex_flush_buffer(ihex, line_buffer, w);
}

// Write out `ihex->data`
//
static void
ihex_write_data (struct ihex_state * const ihex) {
    unsigned int len = ihex->length;
    unsigned int sum = len;
    uint8_t *r = ihex->data;
    char *w = line_buffer;

    if (!len) {
        return;
    }

    if (ihex->flags & IHEX_FLAG_ADDRESS_OVERFLOW) {
        ihex_write_extended_address(ihex, ADDRESS_HIGH_BYTES(ihex->address),
                                    IHEX_EXTENDED_LINEAR_ADDRESS_RECORD);
        ihex->flags &= ~IHEX_FLAG_ADDRESS_OVERFLOW;
    }

    // :
    *w++ = IHEX_START;

    // length
    w = ihex_buffer_byte(w, len);
    ihex->length = 0;

    // 16-bit address
    {
        unsigned int addr = ihex->address & 0xFFFFU;
        ihex->address += len;
        if ((0xFFFFU - addr) < len) {
            // signal address overflow (need to write extended address)
            ihex->flags |= IHEX_FLAG_ADDRESS_OVERFLOW;
        }
        w = ihex_buffer_word(w, addr, &sum);
    }

    // record type
    w = ihex_buffer_byte(w, IHEX_DATA_RECORD);
    //sum += IHEX_DATA_RECORD; // IHEX_DATA_RECORD is zero, so NOP

    // data
    do {
        unsigned int byte = *r++;
        sum += byte;
        w = ihex_buffer_byte(w, byte);
    } while (--len);

    // checksum
    w = ihex_buffer_byte(w, ~sum + 1U);

    w = ihex_buffer_newline(w);
    ihex_flush_buffer(ihex, line_buffer, w);
}

void
ihex_write_at_address (struct ihex_state *ihex, ihex_address_t address) {
    if (ihex->length) {
        // flush any existing data
        ihex_write_data(ihex);
    }
    if ((ihex->address & ADDRESS_HIGH_MASK) != (address & ADDRESS_HIGH_MASK)) {
        ihex->flags |= IHEX_FLAG_ADDRESS_OVERFLOW;
    } else {
        ihex->flags &= ~IHEX_FLAG_ADDRESS_OVERFLOW;
    }
    ihex->address = address;
    ihex_set_output_line_length(ihex, ihex->line_length);
}

void
ihex_set_output_line_length (struct ihex_state *ihex, uint8_t line_length) {
    if (line_length > IHEX_MAX_OUTPUT_LINE_LENGTH) {
        line_length = IHEX_MAX_OUTPUT_LINE_LENGTH;
    } else if (!line_length) {
        line_length = IHEX_DEFAULT_OUTPUT_LINE_LENGTH;
    }
    ihex->line_length = line_length;
}

#ifndef IHEX_DISABLE_SEGMENTS
void
ihex_write_at_segment (struct ihex_state *ihex, ihex_segment_t segment, ihex_address_t address) {
    ihex_write_at_address(ihex, address);
    if (ihex->segment != segment) {
        // clear segment
        ihex_write_extended_address(ihex, (ihex->segment = segment),
                                    IHEX_EXTENDED_SEGMENT_ADDRESS_RECORD);
    }
}
#endif

void
ihex_write_byte (struct ihex_state *ihex, unsigned int byte) {
    if (ihex->line_length <= ihex->length) {
        ihex_write_data(ihex);
    }
    ihex->data[(ihex->length)++] = byte;
}

void
ihex_end_write (struct ihex_state *ihex) {
    ihex_write_data(ihex); // flush any remaining data
    ihex_write_end_of_file(ihex);
}

void
ihex_write_bytes (struct ihex_state *ihex, uint8_t *data, unsigned int count) {
    uint8_t *r = data;
    while (count) {
        if (ihex->line_length > ihex->length) {
            unsigned int i = ihex->line_length - ihex->length;
            uint8_t *w = &(ihex->data[ihex->length]);
            i = (i > count) ? count : i;
            count -= i;
            ihex->length += i;
            do {
                *w++ = *r++;
            } while (--i);
        } else {
            ihex_write_data(ihex);
        }
    }
}

