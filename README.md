![QOI Logo](https://qoiformat.org/qoi-logo.svg)

QOI - The “Quite OK Image Format” for fast, lossless image compression.

More info at https://qoiformat.org.

# QOI targeted to FPGAs

## Verilog

Fully featured encoder and decoder, all QOI_OPs supported according to the spec.

### Testing

Verilator is able to emulate the fpga logic on a computer and is embeddable in C,
enough for it to be a drop in qoi_{encode,decode} replacement for some of the
reference implementation.

Run `make VERILATED=1 test_encode`, it will convert an image to qoi and then use the
system converter to convert back (both ours and the reference .qoi). The
md5sums of the resulting png files should match (it should mean all pixels
survived being encoded), the md5sums of the .qoi files should also match if
it's a full featured encoder.

Run `make VERILATED=1 test_decode` for a similar test exercising the decoding.

## LiteX

Coming soon!

## Connections

### Encoder

#### Input

The `rgba` pixel data with a `clk`.

#### Output

The encoder will usually output a whole QOI `chunk` (1-5 bytes), though delayed
by one clock cycle (to account for any QOI_OP_RUN). If Alpha is not needed
the max chunk size is 4 bytes, which might fit in a single memory transaction.

`chunk_len` specifies how many bytes the current `chunk` contains, could be
0 bytes in case of QOI_OP_RUN for many pixels, could be as high as 5 when
we have a fresh RGBA pixel.

This will eventually need to connect to some kind of bus like AXI or Avalon.
Not really sure how to deal with the variable length output from the encoder yet.

### Decoder

#### Input
A way to peek at the next `chunk` (5 bytes if RGBA, or 4 for just RGB).

For every clock the decoder will output how many bytes it "ate" via
`chunk_len_consumed`,

This value can be used to advance the peek pointer (or sometimes even stay
in the same spot for a while in case of QOI_OP_RUN).

#### Output

One RGBA pixel per clock.
