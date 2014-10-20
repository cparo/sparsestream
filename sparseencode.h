#define _GNU_SOURCE

#include "sparsestream.h"

#define BUFFER_ELEMENTS CHUNK_SIZE / sizeof(uint64_t)

uint64_t buffer[BUFFER_ELEMENTS] = {0};

// Check for whether the buffer contains anything other than zero/null bytes.
// This works with a simple "inline" declaration with no corresponding "extern"
// anywhere else because no object derived from this header is also derived from
// more than one c file using this function (doing so would induce a "multiple
// declaration" error at compile time). Explicitly hinting that this function
// should be inligned isn't of much use to the compiler (which generally is very
// good at making such optimizations on its own when it is appropriate to do
// do), but it does clarify to human readers that the intent for this function
// is merely to avoid source duplication rather than for it to actually be
// compiled as a "real" non-inline function. Compiler hints in more complex
// projects can themselves become complex and subtle to work with - and are
// generally best avoided when not found to offer any significant improvement
// through profiling/testing.
inline int buffer_is_used()
{
	uint64_t i, rc;

	// This check is done 64 bits at a time rather than in the (more intuitive)
	// granularity of one byte at a time because doing so substantially improves
	// the performance of this encoding utility due to the eightfold reduction
	// of comparisons (which, on an x86_64 processor, are done in 64-bit
	// registers anyway). We also use the |= operator to update our return value
	// instead of simply using an if/else to return true upon encountering a
	// non-zero value as this has been found to reduce user-space computational
	// cost by about 25-30% for zero-filled blocks and increase it by about 20%
	// for randomly-filled blocks relative to to the more obvious method (likely
	// because of the avoidance of branching within the loop). Since this
	// encoder is being used because the input data is expected to generally
	// have a high proportion of empty space, favoring reduced overhead for
	// processing zero-filled blocks makes sense.
	for (i = rc = 0; i < BUFFER_ELEMENTS; i++) rc |= buffer[i];
	return rc;
}
