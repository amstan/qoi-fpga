#include <stdio.h>

#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO // We just want the qoi aux internal headers
#include "qoi.h"

#include "verilated.h"
#include "Vqoi_encoder.h"

#define QOI_FPGA_ENCODER_POST_CYCLES 1

void fpga_encode_chunk(Vqoi_encoder *enc, qoi_rgba_t px, unsigned char *bytes, int *p) {
	int i;

	enc->eval();
	enc->r = px.rgba.r;
	enc->g = px.rgba.g;
	enc->b = px.rgba.b;
	enc->a = px.rgba.a;
	enc->eval();
	enc->clk = 0;
	enc->eval();
	enc->clk = 1;
	enc->eval();

	printf("p=%d rgba=%08x %d ", *p, px.v, enc->chunk_len);

	for (i = 0; i < enc->chunk_len; i++) {
		bytes[(*p)++] = enc->chunk[i];
		printf("%02x", enc->chunk[i]);
	}
	printf("|");
	for (;i < 5; i++)
		printf("%02x", enc->chunk[i]);
	printf("\n");
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

	Vqoi_encoder *enc = new Vqoi_encoder;
	enc->rst = 1;
	enc->eval();
	enc->rst = 0;
	enc->eval();

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

		fpga_encode_chunk(enc, px, bytes, &p);
	}

	for (i = 0; i < QOI_FPGA_ENCODER_POST_CYCLES; i++) {
		px.v = 0xfeedbeef; /* send dummy pixels now */
		fpga_encode_chunk(enc, px, bytes, &p);
	}

	for (i = 0; i < (int)sizeof(qoi_padding); i++) {
		bytes[p++] = qoi_padding[i];
	}

	*out_len = p;
	return bytes;
}

void *qoi_decode(const void *data, int size, qoi_desc *desc, int channels) {
	// Not Implemented
	return 0;
}
