/*
 * kk_ihex.h: A simple library to read and write Intel HEX data. Intended
 * mainly for embedded systems, and thus somewhat optimised for size at
 * the expense of error handling and generality.
 *
 *
 *      READING INTEL HEX DATA
 *      ----------------------
 *
 * To read data in the Intel HEX format, you must perform the actual reading
 * of bytes using other means (e.g., stdio). The bytes read must then be
 * passed to `ihex_read_byte` and/or `ihex_read_bytes`. The reading functions
 * will then call `ihex_data_read`, at which stage the `struct ihex_state`
 * structure will contain the data along with its address. See below for
 * details and example implementation of `ihex_data_read`.
 *
 * The sequence to read data in IHEX format is:
 *      struct ihex_state ihex;
 *      ihex_init(&ihex);
 *      ihex_begin_read(&ihex);
 *      ihex_read_bytes(&ihex, my_input_bytes, length_of_my_input_bytes);
 *      ihex_end_read(&ihex);
 *
 *
 *      WRITING BINARY DATA AS INTEL HEX
 *      --------------------------------
 *
 * In order to write out data, the `ihex_write_at_address` or
 * `ihex_write_at_segment` functions are used to set the data location,
 * and then the binary bytes are written with `ihex_write_byte` and/or
 * `ihex_write_bytes`. The writing functions will then call the function
 * `ihex_flush_buffer` whenever the internal write buffer needs to be
 * cleared - it is up to the caller to provide an implementation of
 * `ihex_flush_buffer` to do the actual writing. See below for details
 * and an example implementation.
 *
 * See the declaration further down for an example implementation.
 *
 * The sequence to write data in IHEX format is:
 *      struct ihex_state ihex;
 *      ihex_init(&ihex);
 *      ihex_write_at_address(&ihex, 0);
 *      ihex_write_bytes(&ihex, my_data, length_of_my_data);
 *      ihex_end_write(&ihex);
 *
 * For outputs larger than 64KiB, 32-bit linear addresses are output. Normally
 * the initial linear extended address record of zero is NOT written - it can
 * be forced by setting `ihex->flags |= IHEX_FLAG_ADDRESS_OVERFLOW` before
 * writing the first byte.
 *
 * Gaps in the data may be created by calling `ihex_write_at_address` with the
 * new starting address without calling `ihex_end_write` in between.
 *
 *
 * The same `struct ihex_state` may be used either for reading or writing,
 * but NOT both at the same time. Furthermore, a global output buffer is
 * used for writing, i.e., multiple threads must not write simultaneously
 * (but multiple writes may be interleaved).
 *
 *
 *      CONSERVING MEMORY
 *      -----------------
 *
 * For memory-critical use, you can save additional memory by defining
 * `IHEX_LINE_MAX_LENGTH` as something less than 255. Note, however, that
 * this limit affects both reading and writing, so the resulting library
 * will be unable to read lines with more than this number of data bytes.
 * That said, I haven't encountered any IHEX files with more than 32
 * data bytes per line.
 *
 * If you are using only reading or only writing, the other functionality
 * can be disabled by defining `IHEX_DISABLE_READING` for write-only or
 * `IHEX_DISABLE_WRITING` for read-only. In case of write-only, it is
 * also advantageous to define `IHEX_LINE_MAX_LENGTH` as equal to the line
 * length that you'll be writing (usually 32 or 16).
 *
 *
 * Copyright (c) 2013 Kimmo Kulovesi, http://arkku.com/
 * Provided with absolutely no warranty, use at your own risk only.
 * Use and distribute freely, mark modified copies as such.
 */

#ifndef KK_IHEX_H
#define KK_IHEX_H

#include <stdint.h>
#include <stdbool.h>

typedef uint_least32_t ihex_address_t;
typedef uint_least16_t ihex_segment_t;

// Maximum number of data bytes per line (applies to both reading and
// writing!); specify 255 to support reading all possible lengths. Less
// can be used to limit memory footprint on embedded systems, e.g.,
// most programs with IHEX output use 32.
#ifndef IHEX_LINE_MAX_LENGTH
#define IHEX_LINE_MAX_LENGTH 255
#endif

// Default number of data bytes written per line
#if IHEX_LINE_MAX_LENGTH >= 32
#define IHEX_DEFAULT_OUTPUT_LINE_LENGTH 32
#else
#define IHEX_DEFAULT_OUTPUT_LINE_LENGTH IHEX_LINE_MAX_LENGTH
#endif

// The newline string (appended to every output line)
#ifndef IHEX_NEWLINE_STRING
#define IHEX_NEWLINE_STRING "\n"
#endif

typedef struct ihex_state {
    ihex_address_t address;
    ihex_segment_t segment;
    uint8_t flags;
    uint8_t line_length;
    uint8_t length;
    uint8_t data[IHEX_LINE_MAX_LENGTH + 1];
} kk_ihex_t;

#define IHEX_FLAG_ADDRESS_OVERFLOW  0x80    // 16-bit address overflow

enum ihex_record_type {
    IHEX_DATA_RECORD,
    IHEX_END_OF_FILE_RECORD,
    IHEX_EXTENDED_SEGMENT_ADDRESS_RECORD,
    IHEX_START_SEGMENT_ADDRESS_RECORD,
    IHEX_EXTENDED_LINEAR_ADDRESS_RECORD,
    IHEX_START_LINEAR_ADDRESS_RECORD
};

// Initialise the structure `ihex`
void ihex_init(struct ihex_state * const ihex);

#ifndef IHEX_DISABLE_READING

    /*** INPUT ***/

// Begin reading at address 0
void ihex_begin_read(struct ihex_state *ihex);

// Begin reading at `address` (the lowest 16 bits of which will be ignored);
// this is required only if the high bytes of the 32-bit starting address
// are not specified in the input data and they are non-zero
void ihex_read_at_address(struct ihex_state *ihex, ihex_address_t address);

// Begin reading at `segment`; this is required only if the initial segment
// is not specified in the input data and it is non-zero
void ihex_read_at_segment(struct ihex_state *ihex, ihex_segment_t segment);

// Read a single byte
void ihex_read_byte(struct ihex_state *ihex, char b);

// Read `count` bytes from `data`
void ihex_read_bytes(struct ihex_state *ihex, char *data, unsigned int count);

// End reading (may call `ihex_data_read` if there is data waiting)
void ihex_end_read(struct ihex_state *ihex);

// Called when a complete line has been read, the record type of which is
// passed as `type`. The `ihex` structure will have its fields `data`,
// `line_length`, `address`, and `segment` set appropriately. In case
// of reading an `IHEX_EXTENDED_LINEAR_ADDRESS_RECORD` or an
// `IHEX_EXTENDED_SEGMENT_ADDRESS_RECORD` the record's data is not
// yet parsed - it will be parsed into the `address` or `segment` field
// only if this function returns true.
//
// Possible error cases include checksum mismatch (which is indicated
// as an argument), and excessive line length (in case this has been
// compiled with `IHEX_LINE_MAX_LENGTH` less than 255) which is indicated
// by `line_length` greater than `length`. Unknown record types and
// other erroneous data is usually silently ignored by this minimalistic
// parser. (It is recommended to compute a hash over the complete data
// once received and verify that.)
//
// Example implementation:
//
//      bool ihex_data_read(struct ihex_state *ihex,
//                          enum ihex_record_type type, bool error) {
//          error = error || (ihex->length < ihex->line_length);
//          if (type == IHEX_DATA_RECORD && !error) {
//              (void) fseek(outfile, ihex_linear_address(ihex), SEEK_SET);
//              (void) fwrite(ihex->data, 1, ihex->length, outfile);
//          } else if (type == IHEX_END_OF_FILE_RECORD) {
//              (void) fclose(outfile);
//          }
//          return !error;
//      }
//
extern bool ihex_data_read(struct ihex_state *ihex,
                           enum ihex_record_type type,
                           bool checksum_mismatch);

#endif // !IHEX_DISABLE_READING
#ifndef IHEX_DISABLE_WRITING

    /*** OUTPUT ***/

// Begin writing at the given 32-bit `address`
// (can also be used to skip to a new address without calling
// `ihex_end_write`); set ihex->line_length after calling to
// specify output line length (default 32)
void ihex_write_at_address(struct ihex_state *ihex, ihex_address_t address);

// Write a single byte
void ihex_write_byte(struct ihex_state *ihex, uint8_t b);

// Write `count` bytes from `data`
void ihex_write_bytes(struct ihex_state *ihex, uint8_t *data, unsigned int count);

// End writing (flush buffers, write end of file record)
void ihex_end_write(struct ihex_state *ihex);

// Called whenever the global, internal write buffer needs to be flushed by
// the write functions. The implementation is NOT provided by this library;
// this must be implemented to perform the actual output, i.e., write out
// `(eptr - buffer)` bytes from `buffer` (which is not NUL-terminated, but
// may be modified to make it thus).
//
// Example implementation:
//
//      void ihex_flush_buffer(char *buffer, char *eptr) {
//          *eptr = '\0';
//          (void) fputs(buffer, stdout);
//      }
//
// Note that the contents of `buffer` can become invalid immediately after
// this function returns - the data must be copied if it needs to be preserved!
//
extern void ihex_flush_buffer(struct ihex_state *ihex,
                              char *buffer, char *eptr);

// As `ihex_write_at_address`, but specify a segment selector. Note that
// segments are not automatically incremented when the 16-bit address
// overflows, because the default is to use 32-bit addressing. For segmented
// 20-bit addressing you must manually ensure that a write does not overflow
// the 16-bit address and call `ihex_write_at_segment` every time the segment
// needs to be changed.
void ihex_write_at_segment(struct ihex_state *ihex, ihex_segment_t segment,
                           ihex_address_t address);

// Set the output line length to `length` - may be safely called only right
// after `ihex_write_at_address` or `ihex_write_at_segment`. The maximum
// is IHEX_LINE_MAX_LENGTH (which may be changed at compile time).
void ihex_set_output_line_length(struct ihex_state *ihex, uint8_t line_length);

#endif // !IHEX_DISABLE_WRITING

// Resolve segmented address (if any), return the linear address
ihex_address_t ihex_linear_address(struct ihex_state *ihex);

#endif // !KK_IHEX_H
