/*
 * kk_ihex.c: A simple library to read and write Intel HEX data.
 *
 * See the header `kk_ihex.h` for instructions.
 *
 * Copyright (c) 2013 Kimmo Kulovesi, http://arkku.com/
 * Provided with absolutely no warranty, use at your own risk only.
 * Use and distribute freely, mark modified copies as such.
 */

#include "kk_ihex.h"

static const char IHEX_START = ':';
static const char IHEX_NEWLINE[] = IHEX_NEWLINE_STRING;

#define ADDRESS_HIGH_MASK 0xFFFF0000U
#define ADDRESS_HIGH_BYTES(addr) ((addr) >> 16)

#define HEX_DIGIT(n) ((n) + ( ((n) < 10) ? '0' : ('A' - 10)))

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
    READ_DATA_LOW,
    READ_CHECKSUM_HIGH,
    READ_CHECKSUM_LOW
};

#define IHEX_READ_RECORD_TYPE_MASK 0x07
#define IHEX_READ_STATE_MASK 0x78
#define IHEX_READ_STATE_OFFSET 3

void
ihex_init (struct ihex_state * const ihex) {
    ihex->address = 0;
    ihex->segment = 0;
    ihex->length = 0;
    ihex->flags = 0;
    ihex->line_length = IHEX_DEFAULT_OUTPUT_LINE_LENGTH;
}

#ifndef IHEX_DISABLE_WRITING

static char line_buffer[1+2+4+2+IHEX_LINE_MAX_LENGTH+2+sizeof(IHEX_NEWLINE)];

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
    if (line_length > IHEX_LINE_MAX_LENGTH) {
        line_length = IHEX_LINE_MAX_LENGTH;
    } else if (!line_length) {
        line_length = IHEX_DEFAULT_OUTPUT_LINE_LENGTH;
    }
    ihex->line_length = line_length;
}

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

#endif // !IHEX_DISABLE_WRITING

#ifndef IHEX_DISABLE_READING

void
ihex_begin_read (struct ihex_state *ihex) {
    ihex->line_length = 0;
    ihex->length = 0;
    ihex->flags = 0;
}

void
ihex_read_at_address (struct ihex_state *ihex, ihex_address_t address) {
    ihex_begin_read(ihex);
    ihex->address = address;
}

void
ihex_read_at_segment (struct ihex_state *ihex, ihex_segment_t segment) {
    ihex_begin_read(ihex);
    ihex->address = 0;
    ihex->segment = segment;
}

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
        ihex->data[IHEX_LINE_MAX_LENGTH] ^= 0x100U - (sum & 0xFF);
        len = ihex->length;
    }
    if (ihex_data_read(ihex, type, ihex->data[IHEX_LINE_MAX_LENGTH] != 0)) {
        if (type == IHEX_EXTENDED_LINEAR_ADDRESS_RECORD) {
            ihex_address_t addr = ihex->address & 0xFFFFU;
            addr |= ((ihex_address_t) ihex->data[0]) << 24;
            addr |= ((ihex_address_t) ihex->data[1]) << 16;
            ihex->address = addr;
        } else if (type == IHEX_EXTENDED_LINEAR_ADDRESS_RECORD) {
            ihex->segment = ihex->data[0] << 8 | ihex->data[1];
        }
    }
    ihex_begin_read(ihex);
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
            ihex->data[IHEX_LINE_MAX_LENGTH] = 0xFF;
            break;
        case READ_COUNT_LOW:
            ihex->line_length |= n;
            if (ihex->line_length > IHEX_LINE_MAX_LENGTH) {
                ihex_end_read(ihex);
                return;
            }
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
            if (ihex->line_length == 0 && state == READ_RECORD_TYPE_LOW) {
                state = READ_CHECKSUM_HIGH;
                goto save_read_state;
            }
            break;
        case READ_DATA_HIGH:
            ihex->data[ihex->length] = n;
            break;
        case READ_DATA_LOW: {
            unsigned int len = ihex->length;
            ihex->data[len++] |= n;
            if (len == ihex->line_length) {
                state = READ_CHECKSUM_HIGH;
            } else {
                state = READ_DATA_HIGH;
            }
            ihex->length = len;
            goto save_read_state;
        }
        case READ_CHECKSUM_HIGH:
            ihex->data[IHEX_LINE_MAX_LENGTH] = n;
            break;
        case READ_CHECKSUM_LOW:
            ihex->data[IHEX_LINE_MAX_LENGTH] |= n;
            ihex_end_read(ihex);
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

#endif // !IHEX_DISABLE_READING

ihex_address_t
ihex_linear_address (struct ihex_state *ihex) {
    ihex_address_t address = ihex->address;
    if (ihex->segment) {
        address += ((ihex_address_t) ihex->segment) << 4;
    }
    return address;
}

