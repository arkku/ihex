kk_ihex
=======

A small library for reading and writing Intel HEX (or IHEX) format. See the
header file `kk_ihex.h` for documentation, or below for simple examples.

~ [Kimmo Kulovesi](http://arkku.com/), 2013-12-27

Writing
=======

Basic usage for writing binary data as IHEX ASCII:

    struct ihex_state ihex;
    ihex_init(&ihex);
    ihex_write_at_address(&ihex, 0);
    ihex_write_bytes(&ihex, my_data_bytes, my_data_size);
    ihex_end_write(&ihex);

The actual writing is done by a callback called `ihex_flush_buffer`,
which must be implemented, e.g., as follows:

    void ihex_flush_buffer(struct ihex_state *ihex, char *buffer, char *eptr) {
        *eptr = '\0';
        (void) fputs(buffer, stdout);
    }

For a complete example, see the included program `bin2ihex.c`.

Reading
=======

Basic usage for reading ASCII IHEX into binary data:

    struct ihex_state ihex;
    ihex_init(&ihex);
    ihex_begin_read(&ihex);
    ihex_read_bytes(&ihex, my_ascii_bytes, my_ascii_length);
    ihex_end_read(&ihex);

The reading functions call the function `ihex_data_read`, which must be
implemented by the caller to store the binary data, e.g., as follows;

    bool ihex_data_read (struct ihex_state *ihex,
                         enum ihex_record_type type,
                         bool checksum_error) {
        if (type == IHEX_DATA_RECORD) {
            unsigned long address = (unsigned long) ihex_linear_address(ihex);
            (void) fseek(outfile, address, SEEK_SET);
            (void) fwrite(ihex->data, ihex->length, 1, outfile);
        } else if (type == IHEX_END_OF_FILE_RECORD) {
            (void) fclose(outfile);
        }
        return true;
    }

For an example complete with error handling, see the included program
`ihex2bin.c`.
