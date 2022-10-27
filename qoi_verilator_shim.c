/* Copyright(c) 2021 Dominic Szablewski

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files(the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions :
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

// Contains drop-in replacement of qoi_{encode,decode} that use the verilog
// implementation via verilator.

#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO // We just want the qoi aux internal headers
#include "qoi.h"

#include "verilated.h"
#include "Vqoi_encoder.h"
#include "Vqoi_decoder.h"

#ifdef DEBUG
#include <stdio.h>
#define qoi_debug(...) printf(__VA_ARGS__)
#else
#define qoi_debug(...)
#endif

#define QOI_CHUNK_MAX 5

int fpga_encode_chunk(Vqoi_encoder *v, qoi_rgba_t px, unsigned char *bytes, int *p) {
	int i;

	v->eval();
	v->r = px.rgba.r;
	v->g = px.rgba.g;
	v->b = px.rgba.b;
	v->a = px.rgba.a;
	v->eval();
	v->clk = 0;
	v->eval();
	v->clk = 1;
	v->eval();

	qoi_debug("p=%d rgba=%08x %d ", *p, px.v, v->chunk_len);

	for (i = 0; i < v->chunk_len; i++) {
		bytes[(*p)++] = v->chunk[i];
		qoi_debug("%02x", v->chunk[i]);
	}
	qoi_debug("|");
	for (;i < QOI_CHUNK_MAX; i++)
		qoi_debug("%02x", v->chunk[i]);
	qoi_debug("\n");

	return v->chunk_len;
}

void *qoi_encode(const void *data, const qoi_desc *desc, int *out_len) {
	int i, max_size, p;
	int px_len, px_pos, channels;
	unsigned char *bytes;
	const unsigned char *pixels;
	qoi_rgba_t px;

	if (
		data == NULL || out_len == NULL || desc == NULL ||
		desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 ||
		desc->height >= QOI_PIXELS_MAX / desc->width
	) {
		return NULL;
	}

	max_size =
		desc->width * desc->height * (desc->channels + 1) +
		QOI_HEADER_SIZE + sizeof(qoi_padding);

	p = 0;
	bytes = (unsigned char *) QOI_MALLOC(max_size);
	if (!bytes) {
		return NULL;
	}

	qoi_write_32(bytes, &p, QOI_MAGIC);
	qoi_write_32(bytes, &p, desc->width);
	qoi_write_32(bytes, &p, desc->height);
	bytes[p++] = desc->channels;
	bytes[p++] = desc->colorspace;

	pixels = (const unsigned char *)data;

	Vqoi_encoder *v = new Vqoi_encoder;
	v->rst = 1;
	v->clk = 0;
	v->eval();
	v->clk = 1;
	v->eval();
	v->rst = 0;

	px_len = desc->width * desc->height * desc->channels;
	channels = desc->channels;

	for (px_pos = 0; px_pos < px_len; px_pos += channels) {
		if (channels == 4) {
			px = *(qoi_rgba_t *)(pixels + px_pos);
		}
		else {
			px.rgba.r = pixels[px_pos + 0];
			px.rgba.g = pixels[px_pos + 1];
			px.rgba.b = pixels[px_pos + 2];
			px.rgba.a = 255;
		}

		fpga_encode_chunk(v, px, bytes, &p);
	}
	qoi_debug("last pixel ^^^\n");

	// signal encoder that we're done
	v->finish = 1;
	// but keep reading while there's still chunks in there
	while (fpga_encode_chunk(v, px /* don't care */, bytes, &p));

	for (i = 0; i < (int)sizeof(qoi_padding); i++) {
		bytes[p++] = qoi_padding[i];
	}

	*out_len = p;
	return bytes;
}

int fpga_decode_chunk(Vqoi_decoder *v, const unsigned char chunk[QOI_CHUNK_MAX], unsigned char **pixel_bytes, unsigned char *pixel_end, int channels) {
	int run = 0;

	// Let the decoder peek at the next QOI_CHUNK_MAX chunks
	v->eval();
	for (int i = 0; i < QOI_CHUNK_MAX; i++)
		v->chunk[i] = chunk[i];
	v->eval();

	do {
		// Ask decoder for a pixel
		v->clk = 0;
		v->eval();
		v->clk = 1;
		v->eval();

		// Make sure we don't overflow in the middle of a RUN
		if (*pixel_bytes == pixel_end) break;

		// Copy pixel to our buffer
		(*(*pixel_bytes)++) = v->r;
		(*(*pixel_bytes)++) = v->g;
		(*(*pixel_bytes)++) = v->b;
		if (channels == 4)
			(*(*pixel_bytes)++) = v->a;

		// As long as decoder is not bored of the current chunk and
		// is spewing out pixels.
		run++;
	} while(!v->chunk_len_consumed);

	// Some debugging
	for (int i = 0; i < QOI_CHUNK_MAX; i++)
		if (i < v->chunk_len_consumed)
			qoi_debug("%02x", chunk[i]);
		else
			qoi_debug("  ");
	qoi_debug(" => %02x%02x%02x%02x", v->r, v->g, v->b, v->a);
	if (run > 1) qoi_debug("*%d", run);
	qoi_debug("\n");

	return v->chunk_len_consumed;
}

void *qoi_decode(const void *data, int size, qoi_desc *desc, int channels) {
	const unsigned char *bytes;
	unsigned int header_magic;
	unsigned char *pixels;
	int px_len, chunks_len;
	int p = 0;

	if (
		data == NULL || desc == NULL ||
		(channels != 0 && channels != 3 && channels != 4) ||
		size < QOI_HEADER_SIZE + (int)sizeof(qoi_padding)
	) {
		return NULL;
	}

	bytes = (const unsigned char *)data;

	header_magic = qoi_read_32(bytes, &p);
	desc->width = qoi_read_32(bytes, &p);
	desc->height = qoi_read_32(bytes, &p);
	desc->channels = bytes[p++];
	desc->colorspace = bytes[p++];

	if (
		desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 ||
		header_magic != QOI_MAGIC ||
		desc->height >= QOI_PIXELS_MAX / desc->width
	) {
		return NULL;
	}

	if (channels == 0) {
		channels = desc->channels;
	}

	px_len = desc->width * desc->height * channels;
	pixels = (unsigned char *) QOI_MALLOC(px_len);
	if (!pixels) {
		return NULL;
	}

	chunks_len = size - (int)sizeof(qoi_padding);
	unsigned char *current_pixel = pixels;
	unsigned char *pixel_end = pixels + px_len;

	Vqoi_decoder *v = new Vqoi_decoder;
	v->rst = 1;
	v->clk = 0;
	v->eval();
	v->clk = 1;
	v->eval();
	v->rst = 0;

	while ((p < chunks_len) && (current_pixel < pixel_end)) {
		const unsigned char *current_chunk = bytes + p;

		qoi_debug("%d:", p);
		int chunk_len_consumed =
			fpga_decode_chunk(v, current_chunk, &current_pixel, pixel_end, channels);

		p += chunk_len_consumed;
	}

	return pixels;
}
