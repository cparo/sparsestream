#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Process data in 4KiB chunks. This value is chosen because it is the base
// level of granularity for most popular file systems and the native memory
// page size on x86-family processors.
#define CHUNK_SIZE 4096

int main (int argument_count, char* argument_values[]);
