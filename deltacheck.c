#include "sparsestream.h"

// Buffer checks are done 64 bits at a time rather than in the (more intuitive)
// granularity of one byte at a time because doing so substantially improves
// the performance of this encoding utility due to the eightfold reduction of
// comparisons (which, on an x86_64 processor, are done in 64-bit registers
// anyway). The "CHUNK_SIZE / sizeof(uint64_t)" calculation is composed entirely
// of information known at compile time and will be optimized away to the
// correct scalar value by the compiler - so we can use that here for clarity
// rather than injecting a "magic number" to avoid run-time computation of the
// correct number iterations. There is probably some SIMD trick which would make
// this go even faster, but written as-is the overhead of running this encoding
// utility is negligable.
#define BUFFER_ELEMENTS CHUNK_SIZE / sizeof(uint64_t)

uint64_t base_buffer[BUFFER_ELEMENTS] = {0};
uint64_t rslt_buffer[BUFFER_ELEMENTS] = {0};

static inline int buffer_is_used(uint64_t* buffer)
{
	int i;
	for (i = 0; i < BUFFER_ELEMENTS; i++) {
		if (buffer[i]) return 1;
	}
	return 0;
}

static inline int buffers_differ()
{
	int i;
	for (i = 0; i < BUFFER_ELEMENTS; i++) {
		if (base_buffer[i] != rslt_buffer[i]) return 1;
	}
	return 0;
}

static inline int fill_buffer(uint64_t* buffer, int file_descriptor)
{
	if (buffer_is_used(buffer)) {
		// Re-zero the read buffer for the stream if it is not already
		// zero-filled. We check for it being zero-filled before reinitializing
		// it to minimize memory write load and CPU cache invalidations on the
		// assumption that this program will usually be used on images which are
		// essentially sparse in nature.
		memset(buffer, 0, CHUNK_SIZE);
	}

	// Read into the buffer until it is full, we're out of data in the stream,
	// or we've encountered an error reading from the stream.
	int bytes_read = 0;
	int bytes_left = CHUNK_SIZE;
	while ((bytes_read = read(file_descriptor,
	                          buffer + bytes_read,
	                          bytes_left)) > 0) {
		bytes_left -= bytes_read;
		// If the buffer is full, return. Otherwise, loop back around to get
		// some more data to fill it (or find that we've run out of input to
		// process).
		if (bytes_left == 0) return 0;
	}

	if (bytes_read == 0) {
		// We reached the end of the stream. Return the number of bytes
		// remaining in the buffer as an indication of this.
		return bytes_left;
	} else {
		// Something went wrong reading from the stream. Return a negative
		// value to indicate this.
		return -1;
	}
}

// This program takes as its arguments a base device (or file) and a result
// device (or file), and prints statistics on the degree of difference between
// the base and result devices/files.
int main(int argument_count, char* argument_values[])
{
	int exit_code    = 0;
	int total_chunks = 0;
	int diff_chunks  = 0;
	int batch_mode   = 0;

	if (argument_count < 3) {
		printf("Usage: %s [OPTIONS] BASE_DEVICE RESULT_DEVICE\n",
		       argument_values[0]);
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

	int base = open(argument_values[argument_count - 2], O_RDONLY);
	if (base == -1) {
		return 2;
	}

	int rslt = open(argument_values[argument_count - 1], O_RDONLY);
	if (base == -1) {
		return 2;
	}

	// Loop until one of the input streams is out of data (as indicated by a
	// read() return value of 0) or has encountered an error (as indicated by
	// a negative read() return value).
	int base_bytes_left;
	int rslt_bytes_left;
	while (1) {
		base_bytes_left = fill_buffer(base_buffer, base);
		rslt_bytes_left = fill_buffer(rslt_buffer, rslt);
		if (base_bytes_left < 0) {
			// Error encountered reading base file.
			exit_code = 3;
			break;
		}
		if (rslt_bytes_left < 0) {
			// Error encountered reading result file.
			exit_code = 4;
			break;
		}
		if (base_bytes_left < rslt_bytes_left) {
			// Base file is longer than the result file. We treat this as an
			// error since our delta encoding assumes a result which is equal
			// to or greater in size than the base file.
			exit_code = 5;
			break;
		}
		if (rslt_bytes_left == CHUNK_SIZE) {
			// We've read through the whole result file, so we're done.
			break;
		}
		total_chunks++;
		diff_chunks += buffers_differ();
	}

	if (close(base) < 0 && exit_code == 0) exit_code = 6;
	if (close(rslt) < 0 && exit_code == 0) exit_code = 7;

	// Output...
	float percent_diff = diff_chunks * 100.0 / total_chunks;
	if (batch_mode) {
		// Batch output format:
		// [MiB Differing] [MiB Total] [Percent Differing]%
		printf("%d %d %.1f%%\n", diff_chunks / 256,
		                         total_chunks / 256,
		                         percent_diff);
	} else {
		printf("Differing 4KiB chunks: %11d\n", diff_chunks);
		printf("Total     4KiB chunks: %11d\n", total_chunks);
		printf("\n");
		printf("Differing space (MiB): %11.2f\n", diff_chunks / 256.0);
		printf("Total     space (MiB): %11.2f\n", total_chunks / 256.0);
		printf("\n");
		printf("Percent Differing:     %10.1f%%\n", percent_diff);
	}

	return exit_code;
}
