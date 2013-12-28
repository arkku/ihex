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

static const char IHEX_START = ':';
static const char IHEX_NEWLINE[] = IHEX_NEWLINE_STRING;

#define ADDRESS_HIGH_MASK 0xFFFF0000U
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
    ihex->length = 0;
    ihex->flags = 0;
    ihex->line_length = IHEX_DEFAULT_OUTPUT_LINE_LENGTH;
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

// Write the contents of the data buffer to w, increment address, and reset
// data length. Return pointer to one past the last character written to w
// (w is not NUL-terminated).

static char *
ihex_buffer_data (char *w, struct ihex_state * const ihex) {
    unsigned int len = ihex->length;
    unsigned int sum = len;
    uint8_t *r = ihex->data;

    if (!len) {
        return w;
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
            // signal overflow (need to write extended address record)
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
    sum = 0x100U - (sum & 0xFFU);
    w = ihex_buffer_byte(w, sum);

    return ihex_buffer_newline(w);
}

static char *
ihex_buffer_extended_address (char *w, const ihex_segment_t address,
                              const enum ihex_record_type type) {
    unsigned int sum = type + 2;

    *w++ = IHEX_START;          // :
    w = ihex_buffer_byte(w, 2); // length
    w = ihex_buffer_byte(w, 0); // address msb
    w = ihex_buffer_byte(w, 0); // address lsb
    w = ihex_buffer_byte(w, type); // record type
    w = ihex_buffer_word(w, address, &sum); // high bytes of 32-bit address
    w = ihex_buffer_byte(w, 0x100U - sum); // checksum
    return ihex_buffer_newline(w);
}

// Write an end of file record to w, return a pointer to one past the
// last character written to w (which is not NUL-terminated).

static char *
ihex_buffer_end_of_file (char *w) {
    *w++ = IHEX_START;          // :
    w = ihex_buffer_byte(w, 0); // length
    w = ihex_buffer_byte(w, 0); // address msb
    w = ihex_buffer_byte(w, 0); // address lsb
    w = ihex_buffer_byte(w, IHEX_END_OF_FILE_RECORD); // record type
    w = ihex_buffer_byte(w, 0x100U - IHEX_END_OF_FILE_RECORD); // checksum
    return ihex_buffer_newline(w);
}

static void
ihex_check_address_overflow (struct ihex_state *ihex) {
    if (ihex->flags & IHEX_FLAG_ADDRESS_OVERFLOW) {
        char *w = ihex_buffer_extended_address(line_buffer,
                    ADDRESS_HIGH_BYTES(ihex->address),
                    IHEX_EXTENDED_LINEAR_ADDRESS_RECORD);
        ihex_flush_buffer(ihex, line_buffer, w);
        ihex->flags &= ~IHEX_FLAG_ADDRESS_OVERFLOW;
    }
}

void
ihex_write_at_address (struct ihex_state *ihex, ihex_address_t address) {
    if (ihex->length) {
        ihex_check_address_overflow(ihex);
        // flush any existing data
        char *w = ihex_buffer_data(line_buffer, ihex);
        ihex_flush_buffer(ihex, line_buffer, w);
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
        char *w = ihex_buffer_extended_address(line_buffer,
                    (ihex->segment = segment),
                    IHEX_EXTENDED_SEGMENT_ADDRESS_RECORD);
        ihex_flush_buffer(ihex, line_buffer, w);
    }
}
#endif

void
ihex_write_byte (struct ihex_state *ihex, uint8_t byte) {
    ihex->data[(ihex->length)++] = byte;
    if (ihex->length >= ihex->line_length) {
        ihex_check_address_overflow(ihex);
        char *w = ihex_buffer_data(line_buffer, ihex);
        ihex_flush_buffer(ihex, line_buffer, w);
    }
}

void
ihex_end_write (struct ihex_state *ihex) {
    char *w;
    if (ihex->length) {
        ihex_check_address_overflow(ihex);
        w = ihex_buffer_data(line_buffer, ihex);
        ihex_flush_buffer(ihex, line_buffer, w);
    }
    w = ihex_buffer_end_of_file(line_buffer);
    ihex_flush_buffer(ihex, line_buffer, w);
}

void
ihex_write_bytes (struct ihex_state *ihex, uint8_t *data, unsigned int count) {
    uint8_t *r = data;
    while (count) {
        unsigned int i = ihex->line_length - ihex->length;
        if (i) {
            uint8_t *w = &(ihex->data[ihex->length]);
            i = (i > count) ? count : i;
            count -= i;
            ihex->length += i;
            do {
                *w++ = *r++;
            } while (--i);
        }
        if (ihex->length >= ihex->line_length) {
            char *w;
            ihex_check_address_overflow(ihex);
             w = ihex_buffer_data(line_buffer, ihex);
            ihex_flush_buffer(ihex, line_buffer, w);
        }
    } while (count);
}

