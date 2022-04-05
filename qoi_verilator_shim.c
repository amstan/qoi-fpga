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

#include <stdio.h>

#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO // We just want the qoi aux internal headers
#include "qoi.h"

#include "verilated.h"
#include "Vqoi_encoder.h"
#include "Vqoi_decoder.h"

#define QOI_FPGA_ENCODER_POST_CYCLES 1

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

	printf("p=%d rgba=%08x %d ", *p, px.v, v->chunk_len);

	for (i = 0; i < v->chunk_len; i++) {
		bytes[(*p)++] = v->chunk[i];
		printf("%02x", v->chunk[i]);
	}
	printf("|");
	for (;i < 5; i++)
		printf("%02x", v->chunk[i]);
	printf("\n");

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
	v->eval();
	v->rst = 0;
	v->eval();

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
		}

		fpga_encode_chunk(v, px, bytes, &p);
	}

	for (i = 0; i < QOI_FPGA_ENCODER_POST_CYCLES; i++) {
		px.v = 0xfeedbeef; /* send dummy pixels now */
		fpga_encode_chunk(v, px, bytes, &p);
	}

	for (i = 0; i < (int)sizeof(qoi_padding); i++) {
		bytes[p++] = qoi_padding[i];
	}

	*out_len = p;
	return bytes;
}

int fpga_decode_chunk(Vqoi_decoder *v, const unsigned char *chunk, unsigned char * pixels, int *px_pos, int channels) {
	*(qoi_rgba_t*)(pixels + *px_pos) = {0xff, 0xff, 0xff, 0xff};
	*px_pos += channels;

	return 1;
}

void *qoi_decode(const void *data, int size, qoi_desc *desc, int channels) {
	const unsigned char *bytes;
	unsigned int header_magic;
	unsigned char *pixels;
	int px_len, chunks_len, px_pos;
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
	px_pos = 0;

	Vqoi_decoder *v = new Vqoi_decoder;
	v->rst = 1;
	v->eval();
	v->rst = 0;
	v->eval();

	while ((p < chunks_len) && (px_pos < px_len)) {
		int chunk_len_consumed =
			fpga_decode_chunk(v, bytes + p, pixels, &px_pos, channels);
		p += chunk_len_consumed;
	}

// 	for (px_pos = 0; px_pos < px_len; px_pos += channels) {
// 		if (run > 0) {
// 			run--;
// 		}
// 		else if (p < chunks_len) {
// 			int b1 = bytes[p++];
//
// 			if (b1 == QOI_OP_RGB) {
// 				px.rgba.r = bytes[p++];
// 				px.rgba.g = bytes[p++];
// 				px.rgba.b = bytes[p++];
// 				printf("RGB %02x %02x %02x ", bytes[p-3], bytes[p-2], bytes[p-1]);
// 			}
// 			else if (b1 == QOI_OP_RGBA) {
// 				px.rgba.r = bytes[p++];
// 				px.rgba.g = bytes[p++];
// 				px.rgba.b = bytes[p++];
// 				px.rgba.a = bytes[p++];
// 				printf("RGBA %02x %02x %02x %02x ", bytes[p-4], bytes[p-3], bytes[p-2], bytes[p-1]);
// 			}
// 			else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) {
// 				px = index[b1];
// 				printf("INDEX %d ", b1);
// 			}
// 			else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
// 				px.rgba.r += ((b1 >> 4) & 0x03) - 2;
// 				px.rgba.g += ((b1 >> 2) & 0x03) - 2;
// 				px.rgba.b += ( b1       & 0x03) - 2;
// 				printf("DIFF %d %d %d ", ((b1 >> 4) & 0x03) - 2, ((b1 >> 2) & 0x03) - 2, (b1 & 0x03) - 2);
// 			}
// 			else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
// 				int b2 = bytes[p++];
// 				int vg = (b1 & 0x3f) - 32;
// 				px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
// 				px.rgba.g += vg;
// 				px.rgba.b += vg - 8 +  (b2       & 0x0f);
// 				printf("LUMA %d %d %d ", vg, vg - 8 + ((b2 >> 4) & 0x0f), vg - 8 +  (b2       & 0x0f));
// 			}
// 			else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
// 				run = (b1 & 0x3f);
// 				printf("RUN %d ", run);
// 			}
//
// 			index[QOI_COLOR_HASH(px) % 64] = px;
// 		}
//
// 		if (channels == 4) {
// 			*(qoi_rgba_t*)(pixels + px_pos) = px;
// 		}
// 		else {
// 			pixels[px_pos + 0] = px.rgba.r;
// 			pixels[px_pos + 1] = px.rgba.g;
// 			pixels[px_pos + 2] = px.rgba.b;
// 		}
//
// 		printf("px = %08x\n", px.v);
// 	}

	return pixels;
}
