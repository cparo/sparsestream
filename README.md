SparseStream
============

A set of simple utilities for stream-oriented encoding and decoding of sparse
files for storage, analysis, and transfer.

The encoding used for sparse files is a simple set of offset values followed by
the data for any 4KiB chunk of the original file which contained data other
than zero-filled space. This will not preserve the exact sparse semantics of
the original file (since a chunk explicitly filled with zeros will be left out
of the encoding same as a chunk which was never written to at all), but is done
this way by design to be useful for encoding sparse versions of raw disk
volumes and non-sparse disk images.

More specifically, the encoding for each chunk of data written is as follows:

- A 64-bit, unsigned, little-endian index of the chunk's offset in the file,
  measured in the 4KiB chunk size. In other words, to encode a chunk which
  begins 40960 bytes into the file, this will have a decimal value of "10", and
  be encoded as 0x0A00000000000000.
- A 4096-byte data field containing the contents of the non-sparse chunk.

The last chunk of the input file will be encoded to the output whether it is
whole or partial, and without regard to whether it contains any non-zero data.
This is to ensure that the consumer of the sparse-encoded data stream will
always produce a correct-sized file as a decoding of that stream without having
to know anything in advance about the size of the file to produce or use any
special marker as an indication of the last chunk being the last chunk of
input. Rather, input is encoded such that the consumer needs only to observe
and understand an EOF exception to know it has correctly reached the end of the
input and received all data needed for processing - with no look-ahead or
recall required on the part of the consumer and only a simple record of whether
the last block read was empty required for the sparse-encoding producer to know
whether it needs to write an empty "final" block in the case of an input file
which ends on a 4KiB boundary.

The lack of sophistication in this encoder/decoder pair makes it very light on
CPU and memory usage, and thus an excellent pre-procesor for a more heavyweight
compression method (like gzip or lzo) to reduce the amount of work that has to
be done to produce a compact version of a disk image or other large file which
is likely to contain a substantial amount of "empty" space.

Part of the intent of this code is as an example for teaching c to developers
unfamiliar with the language, using a practical but straightforward problem to
make it easy to focus on the code and what it does. Because of this, more
commentary is included than would be typical for an implementation of this sort
of tool - it helps to explain what is going on and why things are done the way
the are to someone who is not yet sufficiently familiar with the language to
see this in the code itself.

### sparseencode
Takes a file (or "-", to indicate stdin) as input and writes a sparse-encoded
stream to stdout.

### sparsedecode
Reads a sparse-encoded stream from stdin and decodes it as a sparse file at the
specified path (or to stdout if "-" is specified as the output path, in which
case the "sparse" chunks will be output as 4KiB sequences of zeros).

### sparsefilter
Reads from a file and writes a sparse version of it at the specified path,
without having to start a sparseencode and sparsedecode process with a pipe to
join them.

### sparsecheck
Takes a file (or "-", to indicate stdin) as input and prints statistics on
the degree to which the input is "sparse" as would be encoded by the
`sparseencode` utility.
