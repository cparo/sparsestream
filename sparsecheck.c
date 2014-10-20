#include "sparseencode.h"

// This program takes a source device (or file) as its sole input argument.
int main(int argument_count, char* argument_values[])
{
	int bytes_left       = CHUNK_SIZE;
	int bytes_read       = 0;
	int last_buffer_used = 0;
	int exit_code        = 0;

	int used_chunks      = 0;
	int free_chunks      = 0;

	int batch_mode       = 0;

	if (argument_count < 2) {
		printf("Usage: %s [OPTIONS] DEVICE\n", argument_values[0]);
		return 1;
	}

	int c;
	while ((c = getopt(argument_count, argument_values, "b:")) != -1) {
		switch (c) {
			case 'b':
				batch_mode = 1;
				break;
			default:
				return 1;
		}
	}

	// If the target "device" is specified as "-", then follow the convention of
	// this indicating stdin.
	int source;
	if (!strcmp(argument_values[argument_count - 1], "-")) {
		source = STDIN_FILENO;
	} else {
		source = open(argument_values[argument_count - 1], O_RDONLY);
		if (source == -1) {
			return 2;
		}
	}

	// Read in data until we've reached the end of our input device/file (as
	// indicated by a read() return value of 0) or encountered an error (as
	// indicated by a negative read() return value).
	while ((bytes_read = read(source,
	                          buffer + bytes_read,
	                          bytes_left)) > 0) {
		bytes_left -= bytes_read;
		// If the buffer is full, process it. Otherwise, loop back around to
		// get some more data to fill it (or find that we've run out of input
		// to process).
		if (bytes_left == 0) {
			// Check buffer and output index and contents if non-zero contents
			// found.
			if (buffer_is_used()) {
				used_chunks++;
				// We only need to re-zero the buffer if it is not already
				// zero-filled.
				memset(buffer, 0, CHUNK_SIZE);
			} else {
				free_chunks++;
			}
			bytes_read = 0;
			bytes_left = CHUNK_SIZE;
		}
	}

	if (bytes_read == 0) {
		// We've reached the end of the input file.
		if (bytes_left == CHUNK_SIZE) {
			// The input file's size was an integer multiple of the chunk size.
			if (!last_buffer_used) {
				// The last chunk of the input file was empty, so we would need
				// to write out a chunk of zeros at the end of the output stream
				// to give the file resulting from that stream's decoding the
				// correct end position. As such, the last chunk should be
				// counted as used space rather than free space even though it
				// was zero-filled.
				used_chunks++;
				free_chunks--;
			}
		} else {
			// The file ended with a partial chunk, which should be counted.
			used_chunks++;
		}
	} else {
		// Something went wrong while reading the input file.
		exit_code = 4;
	}

	if (close(source) < 0 && exit_code == 0) exit_code = 5;

	// Output...
	int total_chunks = used_chunks + free_chunks;
	float percent_used = used_chunks * 100.0 / total_chunks;
	if (batch_mode) {
		// Batch output format:
		// [MiB Used] [MiB Free] [MiB Total] [Percent Used]%
		printf("%d %d %d %.1f%%\n", used_chunks  / 256,
		                            free_chunks  / 256,
		                            total_chunks / 256,
		                            percent_used);
	} else {
		printf("Used 4KiB chunks: %11d\n", used_chunks);
		printf("Free 4KiB chunks: %11d\n", free_chunks);
		printf("\n");
		printf("Used space (MiB): %11.2f\n", used_chunks / 256.0);
		printf("Free space (MiB): %11.2f\n", free_chunks / 256.0);
		printf("\n");
		printf("Percent Used:     %10.1f%%\n", percent_used);
	}

	return exit_code;
}
