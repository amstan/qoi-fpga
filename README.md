![QOI Logo](https://qoiformat.org/qoi-logo.svg)

# QOI targeted to FPGAs!

## Verilog

### Testing

Verilator will be able to adapt the functionality enough for it to be a
drop in replacement for some of the reference implementation.

Run `make VERILATED=1 test`, it will convert an image to qoi and then use the
system converted to convert back (both ours and another reference .qoi). The
md5sums of the resulting png files should match (it should mean all pixels
survived being encoded), the md5sums of the .qoi files ideally should also
match if it's a full featured encoder.

## LiteX

Coming soon!

## Connections

### Encoder

#### Input

This is just the RGB pixel data with a clock.

Let's hardcode it to just RGB to keep things simple.

#### Output

This will eventually need to connect to some kind of bus like AXI or Avalon.
Not really sure how to deal with the variable length output from the encoder.

For now we'll just have the output bytes with a length and some kind of ready
signal. I'm sure that'll be adaptable somehow.
