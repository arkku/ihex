kk_ihex
=======

A small library for reading and writing the
[Intel HEX](http://en.wikipedia.org/wiki/Intel_HEX) (or IHEX) format. The
library is mainly intended for embedded systems and microcontrollers, such
as Arduino, AVR, PIC, ARM, STM32, etc - hence the emphasis is on small size
rather than features, generality, or error handling.

See the header file `kk_ihex.h` for documentation, or below for simple examples.

~ [Kimmo Kulovesi](http://arkku.com/), 2013-12-27

Writing
=======

Basic usage for writing binary data as IHEX ASCII:

    #include "kk_ihex_write.h"
     
    struct ihex_state ihex;
    ihex_init(&ihex);
    ihex_write_at_address(&ihex, 0);
    ihex_write_bytes(&ihex, my_data_bytes, my_data_size);
    ihex_end_write(&ihex);

The function `ihex_write_bytes` may be called multiple times to pass any
amount of data at a time.

The actual writing is done by a callback called `ihex_flush_buffer`,
which must be implemented, e.g., as follows:

    void ihex_flush_buffer(struct ihex_state *ihex, char *buffer, char *eptr) {
        *eptr = '\0';
        (void) fputs(buffer, stdout);
    }

The length of the buffer can be obtained from `eptr - buffer`. The actual
implementation may of course do with the IHEX data as it pleases, e.g.,
transmit it over a serial port.

For a complete example, see the included program `bin2ihex.c`.


Reading
=======

Basic usage for reading ASCII IHEX into binary data:

    #include "kk_ihex_read.h"
     
    struct ihex_state ihex;
    ihex_begin_read(&ihex);
    ihex_read_bytes(&ihex, my_ascii_bytes, my_ascii_length);
    ihex_end_read(&ihex);

The function `ihex_read_bytes` may be called multiple times to pass any
amount of data at a time.

The reading functions call the function `ihex_data_read`, which must be
implemented by the caller to store the binary data, e.g., as follows:

    ihex_bool_t ihex_data_read (struct ihex_state *ihex,
                                ihex_record_type_t type,
                                ihex_bool_t checksum_error) {
        if (type == IHEX_DATA_RECORD) {
            unsigned long address = (unsigned long) IHEX_LINEAR_ADDRESS(ihex);
            (void) fseek(outfile, address, SEEK_SET);
            (void) fwrite(ihex->data, ihex->length, 1, outfile);
        } else if (type == IHEX_END_OF_FILE_RECORD) {
            (void) fclose(outfile);
        }
        return true;
    }

Of course an actual implementation is free to do with the data as it chooses,
e.g., burn it on an EEPROM instead of writing it to a file.

For an example complete with error handling, see the included program
`ihex2bin.c`.


Example Programs
================

The included example programs, `ihex2bin` and `bin2ihex`, implement
a very simple conversion between raw binary data and Intel HEX.
Usage by example:

    # Simple conversion from binary to IHEX:
    bin2ihex <infile.bin >outfile.hex

    # Add an offset to the output addresses (i.e., make the address
    # of the first byte of the input other than zero):
    bin2ihex -a 0x8000000 -i infile.bin -o outfile.hex

    # Simple conversion from IHEX to binary:
    ihex2bin <infile.hex >outfile.bin

    # Manually specify the initial address written (i.e., subtract
    # an offset from the input addresses):
    ihex2bin -a 0x8000000 -i infile.hex -o outfile.bin

    # Start output at the first data byte (i.e., make the address offset
    # equal to the address of the first data byte read from input):
    ihex2bin -A -i infile.hex -o outfile.bin

Both programs also accept the option `-v` to increase verbosity.

When using `ihex2bin` on Intel HEX files produced by compilers and such,
it is a good idea to specify the command-line option `-A` to autodetect
the address offset. Otherwise the program will simply fill any unused
addresses, starting from 0, with zero bytes, which may total mega- or
even gigabytes.

