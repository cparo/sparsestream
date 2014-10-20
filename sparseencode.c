#include "sparseencode.h"

uint64_t chunk_index = 0;
int      source;

// Wrapper for writing out some specific number of bytes of data until the write
// has been completed, as the write() call may return before it has written all
// of the data that it has been asked to write.
static inline void write_all(void *start, int count)
{
	int bytes_written = 0;

	while (bytes_written < count) {
		bytes_written += write(STDOUT_FILENO,
		                       start + bytes_written,
		                       count - bytes_written);
	}
}

// This program takes a source device (or file) as its sole input argument.
int main(int argument_count, char* argument_values[])
{
	int bytes_left       = CHUNK_SIZE;
	int bytes_read       = 0;
	int last_buffer_used = 0;
	int exit_code        = 0;

	if (argument_count != 2) {
		printf("Usage: %s DEVICE\n", argument_values[0]);
		return 1;
	}

	// If the target "device" is specified as "-", then follow the convention of
	// this indicating stdin.
	if (!strcmp(argument_values[1], "-")) {
		source = STDIN_FILENO;
	} else {
		source = open(argument_values[1], O_RDONLY);
		if (source == -1) return 2;
	}

	// Read in data until we've reached the end of our input device/file (as
	// indicated by a read() return value of 0) or encountered an error (as
	// indicated by a negative read() return value).
	while ((bytes_read = read(source,
	                          buffer + bytes_read,
	                          bytes_left)) > 0) {
		// If the buffer is full (there are no bytes left in the buffer beyond
		// what we have read into it), process it. Otherwise, loop back around
		// to get some more data to fill it (or find that we've run out of
		// input to process).
		if (!(bytes_left -= bytes_read)) {
			// Check buffer and output index and contents if non-zero contents
			// found.
			if (buffer_is_used()) {
				last_buffer_used = 1;
				write_all(&chunk_index,  sizeof(chunk_index));
				write_all(buffer, CHUNK_SIZE);
			} else {
				last_buffer_used = 0;
			}
			bytes_read = 0;
			bytes_left = CHUNK_SIZE;
			chunk_index++;
		}
	}

	if (bytes_read == 0) {
		// We've reached the end of the input file.
		if (bytes_left == CHUNK_SIZE) {
			// The input file's size was an integer multiple of the chunk size.
			if (!last_buffer_used) {
				// The last chunk of the input file was empty, so we need to
				// write out a chunk of zeros at the end of the output stream
				// to give the file resulting from that stream's decoding the
				// correct end position.
				chunk_index--;
				write_all(&chunk_index,  sizeof(chunk_index));
				write_all(buffer, CHUNK_SIZE);
			}
		} else {
			// The file ended with a partial chunk, which must now be written
			// to the output stream.
			write_all(&chunk_index,  sizeof(chunk_index));
			write_all(buffer, CHUNK_SIZE - bytes_left);
		}
	} else {
		// Something went wrong while reading the input file.
		exit_code = 4;
	}

	if (close(source) < 0 && exit_code == 0) exit_code = 5;
	return exit_code;
}
