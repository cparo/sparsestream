#include "sparsestream.h"

uint64_t buffer[CHUNK_SIZE / sizeof(uint64_t)] = {0};
uint64_t last_chunk_index = 0;
uint64_t chunk_index;
int      write_offset = 0;
int      target;

// Read in the start index of a chunk as a 64-bit unsigned integer and return
// either the number of bytes successfully read (if no error was encountered)
// or the negative value corresponding to the read error encountered (if an
// error was encountered).
static inline int read_chunk_index()
{
	int bytes_left = sizeof(uint64_t);
	int bytes_read = 0;

	// This is ugly because read() may not give us all eight bytes at once, so
	// we have to check and loop until we have the whole chunk index value.
	while ((bytes_read = read(STDIN_FILENO,
	                          &chunk_index + sizeof(uint64_t) - bytes_left,
	                          bytes_left))) {
		if (bytes_read < 0) {
			perror("Error reading index");
			return bytes_read;
		}
		bytes_left -= bytes_read;
	}
	return bytes_left;
}

// Read in a chunk of data and return either the number of bytes successfully
// read (if no error was encountered) or the negative value corresponding to
// the read error encountered (if an error was encountered).
static inline int read_chunk()
{
	int bytes_left = CHUNK_SIZE;
	int bytes_read = 0;

	while ((bytes_read = read(STDIN_FILENO,
	                          (void*)buffer + CHUNK_SIZE - bytes_left,
	                          bytes_left))) {
		if (bytes_read < 0) {
			perror("Error reading chunk");
			return bytes_read;
		}
		bytes_left -= bytes_read;
	}
	return bytes_left;
}

// Write out "count" bytes of data from our buffer, retrying until all of the
// specified bytes of data have been written.
static inline void write_out_buffer(int count)
{
	int bytes_written = 0;

	while (bytes_written < count) {
		bytes_written += write(target,
		                       buffer + bytes_written,
		                       count - bytes_written);
	}
}

// Advance to the position of the next data chunk for which a write is to be
// performed, either by seeking (in the normal case of a normal file) or by
// writing out chunks of zeroes until any gap has been crossed (in the case of
// writing to stdout).
static inline int advance_to_chunk_index()
{
	if (target == STDOUT_FILENO) {
		while (chunk_index > last_chunk_index) {
			memset(buffer, 0, CHUNK_SIZE);
			write_out_buffer(CHUNK_SIZE);
			last_chunk_index++;
		}
		return 0;
	} else {
		// We can ignore (and ignore updating) the last_chunk_index in this case
		// since it is only relevant for the case where we are writing to stdout
		// and need to expand all "discarded" chunks into chunks of actual
		// zeroes.
		return lseek(target,
		             chunk_index * CHUNK_SIZE + write_offset,
		             SEEK_SET) < 0;
	}
}

// This program takes a target device (or file) and, optionally, an offset to
// seek to before writing as its arguments. If the target device is specified
// as "-", then we will follow the convention of this indicating stdout as the
// write target - and write out the file in an unsparse (with all zeroed chunks
// explicitly written out rather than seeking past them) manner. Otherwise, we
// open the file/device with the specified name (creating a new file if
// necessary) and write out the sparse-encoded input stream by seeking past the
// unused chunks.
int main(int argument_count, char* argument_values[])
{
	int bytes_left;

	if (argument_count != 2 && argument_count != 3) {
		printf("Usage: %s DEVICE [WRITE_OFFSET]\n", argument_values[0]);
		return 1;
	}

	// Check for the special case of specifying "-" to write to stdout.
	if (!strcmp(argument_values[1], "-")) {
		target = STDOUT_FILENO;
	} else {
		target = open(argument_values[1],
		              O_WRONLY | O_CREAT,
		              S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
		if (target == -1) {
			return 2;
		}
	}

	if (argument_count == 3) {
		// TODO: Some sanity checking here would be appropriate.
		write_offset = atoi(argument_values[2]);
		// If we are writing to stdout, we need to start by writing zeros to
		// establish our write offset. For the non-stdout case, this offset
		// can simply be added to any seek() locations.
		if (target == STDOUT_FILENO) {
			memset(buffer, 0, CHUNK_SIZE);
			while (write_offset >= CHUNK_SIZE) {
				write_out_buffer(CHUNK_SIZE);
				write_offset -= CHUNK_SIZE;
			}
			write_out_buffer(write_offset);
		}
	}

	while(1) {
		bytes_left = read_chunk_index();
		if (bytes_left) {
			if (bytes_left == sizeof(chunk_index)) {
				// We've reached the end of the input stream.
				return close(target) ? 6 : 0;
			} else {
				// The input appears to be broken, since we just read part of a
				// chunk index value or got an error code when attempting to
				// read the chunk index value.
				close(target);
				return 3;
			}
		}

		if (advance_to_chunk_index()) {
			// An error has occurred when seeking within the target file.
			close(target);
			return 4;
		}

		bytes_left = read_chunk();
		if (bytes_left < 0) {
			// The input appears to be broken, since we just got an error code
			// when attempting to read a chunk of data.
			close(target);
			return 5;
		} else {
			// Only write out as many bytes as we read in.
			write_out_buffer(CHUNK_SIZE - bytes_left);
		}
	}
}
