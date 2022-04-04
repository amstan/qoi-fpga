![QOI Logo](https://qoiformat.org/qoi-logo.svg)

QOI - The “Quite OK Image Format” for fast, lossless image compression.

More info at https://qoiformat.org.

# QOI targeted to FPGAs

## Verilog

### Testing

Verilator will be able to adapt the functionality enough for it to be a
drop in replacement for some of the reference implementation.

Run `make VERILATED=1 test`, it will convert an image to qoi and then use the
system converter to convert back (both ours and another reference .qoi). The
md5sums of the resulting png files should match (it should mean all pixels
survived being encoded), the md5sums of the .qoi files should also match if
it's a full featured encoder.

## LiteX

Coming soon!

## Connections

### Encoder

100% Implemented.

#### Input

This is just the RGBA pixel data with a clock.

#### Output

The encoder will usually output a whole QOI chunk (1-5 bytes), though delayed
by one clock cycle (to account for any QUI_OP_RUN). If Alpha is not needed
the max chunk size is 4 bytes, which might fit in a single memory transaction.

This will eventually need to connect to some kind of bus like AXI or Avalon.
Not really sure how to deal with the variable length output from the encoder yet.

### Decoder

TODO
