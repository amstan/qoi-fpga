# MQOI - Motion QOI

QOI is a great image format for simplicity. But what if you want a
video or you want to stream multiple frames at the same time?

Similar to how JPG was expanded to MJPEG, we could do the same with QOI!
Just encode a bunch of images, contatenate them together and that's our
new image stream!

Note: For now assume 3 channel output only. TODO

## Applications
* lossless screen streams in the browser
* display stream compressing to embedded displays

## Inter-frame compression
Similar to APNG we can do some compression across frames:
http://littlesvr.ca/apng/inter-frame.html

The idea here is to use the alpha channel as a "mux" between a new pixel or
the previous frame's pixel.

### Decoder
simply overlay the new incoming frame on top of the
previously decoded frame.

### Encoder
On the encoder side: to generate a p-frame calculate the difference between
the previous frame and the current frame, use alpha as the "strength" of the
difference (0 means keep previous pixel, 255 means use 100% of the new pixel).

The complication here are that there are many RGBA solutions that will compose
to the correct pixel. TODO...

## Other ideas

### Keep index across frames
It might be more efficient to reuse the QOI_COLOR_HASH index across frames.
More testing to commence if this is worth breaking backwards compatibility for.
