#include "sparseencode.h"

uint64_t chunk_index = 0;
int      write_offset = 0;
int      source;
int      target;

// Seek to the position of the next data chunk for which a write is to be
// performed.
static inline int advance_to_chunk_index()
{
	return lseek(target,
	             chunk_index * CHUNK_SIZE + write_offset,
	             SEEK_SET) < 0;
}

// Write out "count" bytes of data from our buffer, retrying until all of the
// specified bytes of data have been written.
static int write_out_buffer(int count)
{
	int rc, bytes_written = 0;

	if (advance_to_chunk_index()) return 1;
	while (bytes_written < count) {
		rc = write(target, buffer + bytes_written, count - bytes_written);
		if (rc < 0) return 1;
		bytes_written += rc;
	}
	return 0;
}

// This program takes source and target devices (or file) as its input
// arguments. It will read from stdin if "-" is supplied for its source
// argument.
int main(int argument_count, char* argument_values[])
{
	int bytes_left       = CHUNK_SIZE;
	int bytes_read       = 0;
	int last_buffer_used = 0;
	int exit_code        = 0;

	if (argument_count != 3) {
		printf("Usage: %s INPUT_FILE OUTPUT_FILE\n", argument_values[0]);
		return 1;
	}

	// If the source "device" is specified as "-", then follow the convention
	// of this indicating stdin.
	if (!strcmp(argument_values[1], "-")) {
		source = STDIN_FILENO;
	} else {
		source = open(argument_values[1], O_RDONLY);
		if (source == -1) return 2;
	}

	// It would make no sense whatsoever to run this filter to stdout and put
	// all the zeros we took out back in, so this should only be used with
	// actual, seekable files/devices on the target side.
	target = open(argument_values[2],
	              O_WRONLY | O_CREAT,
	              S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (target == -1) {
		close(source);
		return 3;
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
				last_buffer_used = 1;
				if(write_out_buffer(CHUNK_SIZE)) {
					close(source);
					close(target);
					return 4;
				}
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
				// write out a chunk of zeros at the end of the output file
				// to ensure that it is not shorter than it should be.
				chunk_index--;
				if (write_out_buffer(CHUNK_SIZE)) {
					close(source);
					close(target);
					return 4;
				}
			}
		} else {
			// The file ended with a partial chunk, which must now be written
			// to the output file.
			if (write_out_buffer(CHUNK_SIZE - bytes_left)) {
				close(source);
				close(target);
				return 4;
			}
		}
	} else {
		// Something went wrong while reading the input file.
		exit_code = 5;
	}

	if (close(source) < 0 && exit_code == 0) exit_code = 6;
	if (close(target) < 0 && exit_code == 0) exit_code = 7;
	return exit_code;
}
