/*
 * The JPEG encoder (0x81fa30-0x81ff44): baseline JPEG, or motion JPEG
 * without Huffman tables, from YCbCr 4:2:0 or 4:2:2 planes. The headers and
 * the tables are written here, the MCU rows by the VLIW code.
 */
#include "jpeg.h"

#define ERROR_MEMORY			(-10001)

/* SOI, APP0 "JFIF" 1.2 (or "AVI1"), and the DQT marker before the tables */
static const uint8_t headerStart[] = {
	0xff, 0xd8,
	0xff, 0xe0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0x00, 0x01, 0x02, 0x01, 0x00, 0x80,
	0x00, 0x80, 0x00, 0x00,
	0xff, 0xdb, 0x00, 0x84,
};

/* DHT with the standard tables (ITU T.81 K.3), then SOF0 up to its height */
static const uint8_t huffmanTablesFrameStart[] = {
	0xff, 0xc4, 0x01, 0xa2,
	/* luminance DC */
	0x00,
	0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
	/* luminance AC */
	0x10,
	0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00,
	0x01, 0x7d,
	0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51,
	0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1,
	0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18,
	0x19, 0x1a, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
	0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75,
	0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92,
	0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
	0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
	0xd9, 0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2,
	0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa,
	/* chrominance DC */
	0x01,
	0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
	/* chrominance AC */
	0x11,
	0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01,
	0x02, 0x77,
	0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07,
	0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09,
	0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25,
	0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56,
	0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74,
	0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
	0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
	0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
	0xd7, 0xd8, 0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2,
	0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa,
	/* SOF0, 8 bits */
	0xff, 0xc0, 0x00, 0x11, 0x08,
};

/* SOF0 up to its height, motion JPEG */
static const uint8_t frameStart[] = {
	0xff, 0xc0, 0x00, 0x11, 0x08,
};

/* the rest of SOF0 (Y 2x2, Cb, Cr) and the SOS of the three */
static const uint8_t frameEnd[] = {
	0x03, 0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01,
	0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00,
};

/* the standard quantization tables (ITU T.81 K.1), in zigzag order */
static const uint8_t baseQuantTable[2][64] = {
	{
		16, 11, 12, 14, 12, 10, 16, 14, 13, 14, 18, 17, 16, 19, 24, 40,
		26, 24, 22, 22, 24, 49, 35, 37, 29, 40, 58, 51, 61, 60, 57, 51,
		56, 55, 64, 72, 92, 78, 64, 68, 87, 69, 55, 56, 80, 109, 81, 87,
		95, 98, 103, 104, 103, 62, 77, 113, 121, 112, 100, 120, 92, 101, 103, 99,
	}, {
		17, 18, 18, 24, 21, 24, 47, 26, 26, 47, 99, 66, 56, 66, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
	},
};

/* from each reciprocal to the next in zigzag order, the table transposed */
static const int8_t reciprocalStep[63] = {
	-14, 2, 14, 14, 16, -14, -14, -14, 2, 14, 14, 14, 14, 16, -14, -14,
	-14, -14, -14, 2, 14, 14, 14, 14, 14, 14, 16, -14, -14, -14, -14, -14,
	-14, -14, 16, 14, 14, 14, 14, 14, 14, 2, -14, -14, -14, -14, -14, 16,
	14, 14, 14, 14, 2, -14, -14, -14, 16, 14, 14, 2, -14, 16, 0,
};

/* the output buffer, all but its last 8 bytes */
static int32_t setOutput(JpegEncoderContext *context, uint8_t *buffer, uint32_t size)
{
	if (context == NULL || ((uint32_t)context & 3) || buffer == NULL || size < 8 ||
		(((uint32_t)buffer | size) & 0xff))
		return SCE_JPEGENC_ERROR_INVALID_POINTER;
	context->position = buffer;
	context->end = buffer + size - 8;
	return 0;
}

static void setPlane(JpegEncoderPlane *plane, uint32_t component, uint32_t blocks,
	uint32_t pitch, int32_t step0, int32_t step1, int32_t step2, int32_t step3,
	const uint8_t *row, uint32_t rowStep)
{
	plane->pitch = pitch;
	plane->blocks = blocks;
	plane->component = component;
	plane->block = NULL;
	plane->blockStep[0] = step0;
	plane->blockStep[1] = step1;
	plane->blockStep[2] = step2;
	plane->blockStep[3] = step3;
	plane->row = row;
	plane->rowStep = rowStep;
}

/*
 * A DQT table of the compression ratio (0-128 scale the standard table by
 * ratio / 128, 129-255 more steeply), with the reciprocals for the encoder
 * when the ratio changed.
 */
static uint8_t *writeQuantTable(JpegEncoderContext *context, uint32_t id, uint8_t *p)
{
	uint16_t *reciprocal = context->quantReciprocal[id];
	uint32_t remake = reciprocal[0] == 0 || context->tablesCompRatio != context->compRatio;
	int32_t scale = context->compRatio, quant, k;

	if (scale >= 129)
		scale = scale * 2 - 128;
	*p++ = id;
	for (k = 0; k < 64; k++) {
		quant = (baseQuantTable[id][k] * scale + 192) >> 7;
		if (quant > 255)
			quant = 255;
		*p++ = quant;
		if (remake) {
			*reciprocal = (quant / 2 + 16384) / quant;
			reciprocal = (uint16_t *)((uint8_t *)reciprocal +
				(k ? reciprocalStep[k - 1] : 16));
		}
	}
	return p;
}

static uint8_t *copyBytes(uint8_t *to, const uint8_t *from, uint32_t size)
{
	uint8_t *end = to + size;

	do
		*to++ = *from++;
	while (to != end);
	return to;
}

/* the headers up to the scan's data */
static uint8_t *writeHeaders(JpegEncoderContext *context)
{
	uint32_t height = context->height, width = context->width;
	uint8_t *p;

	p = copyBytes(context->position, headerStart, sizeof(headerStart));
	if (context->headerMode) {
		p[-18] = 'A';
		p[-17] = 'V';
		p[-16] = 'I';
		p[-15] = '1';
	}
	p = writeQuantTable(context, 0, p);
	p = writeQuantTable(context, 1, p);
	context->dcPredictor[2] = 0;
	context->dcPredictor[1] = 0;
	context->dcPredictor[0] = 0;
	if (context->headerMode)
		p = copyBytes(p, frameStart, sizeof(frameStart));
	else
		p = copyBytes(p, huffmanTablesFrameStart, sizeof(huffmanTablesFrameStart));
	p[0] = height >> 8;
	p[1] = height;
	p[2] = width >> 8;
	p[3] = width;
	p = copyBytes(p + 4, frameEnd, sizeof(frameEnd));
	if ((context->pixelFormat & 9) == SCE_JPEGENC_PIXEL_YCBCR422)
		p[-22] = 0x21;		/* Y 2x1 */
	return p;
}

/* the bits left, the last byte padded as the encoder left it */
static int32_t flushBits(JpegEncoderContext *context)
{
	uint8_t *p = context->position;
	uint32_t bits = context->bitBuffer;
	int32_t shift, i;

	if (p >= context->end)
		return SCE_JPEGENC_ERROR_INSUFFICIENT_BUFFER;
	for (i = 0, shift = 24; shift >= 0; i++, shift -= 8) {
		p[i] = bits >> shift;
		if (p[i] == 0xff) {
			p[i + 1] = 0;
			p++;
		}
	}
	context->position = p + ((context->bitCount + 39) >> 3);
	return 0;
}

/* the image at input encoded: the size of the JPEG written */
static int32_t encode(JpegEncoderContext *context, const uint8_t *input)
{
	JpegEncoderContext local;
	JpegEncoderPlane plane[4];
	int32_t columns, rows, frameRows, row, result, k;
	uint32_t pitch, chromaPitch;
	uint8_t *start;

	if (input == NULL || ((uint32_t)input & 7))
		return SCE_JPEGENC_ERROR_INVALID_POINTER;
	memcpy(&local, context, sizeof(local));
	if (local.end - local.position < 610)
		return SCE_JPEGENC_ERROR_INSUFFICIENT_BUFFER;

	pitch = local.pitch;
	columns = (local.width + 15) >> 4;
	if ((local.pixelFormat & 9) == SCE_JPEGENC_PIXEL_YCBCR422) {
		frameRows = local.frameHeight >> 3;
		rows = (local.height + 7) >> 3;
		setPlane(&plane[0], 0, 2, pitch, 8, 8, 0, 0, input, pitch * 8);
	} else {
		frameRows = local.frameHeight >> 4;
		rows = (local.height + 15) >> 4;
		setPlane(&plane[0], 0, 4, pitch, 8, pitch * 8 - 8, 8, 8 - pitch * 8, input,
			pitch * 16);
	}
	chromaPitch = (local.pitch >> 1) + local.chromaPitchExtra;
	setPlane(&plane[1], 1, 1, chromaPitch, 8, 0, 0, 0,
		plane[0].row + plane[0].rowStep * frameRows, chromaPitch * 8);
	setPlane(&plane[2], 2, 1, chromaPitch, 8, 0, 0, 0,
		plane[1].row + plane[1].rowStep * frameRows, chromaPitch * 8);
	plane[3].blocks = 0;

	start = local.position;
	local.bitCount = -32;
	local.bitBuffer = 0;
	local.position = writeHeaders(&local);
	for (row = 0; row < rows; row++) {
		if (local.position >= local.end) {
			result = SCE_JPEGENC_ERROR_INSUFFICIENT_BUFFER;
			goto done;
		}
		for (k = 0; k < 3; k++)
			plane[k].block = plane[k].row;
		jpegEncodeMcuRow(&local, columns, plane);
		for (k = 0; k < 3; k++)
			plane[k].row += plane[k].rowStep;
	}
	result = flushBits(&local);
	if (result < 0)
		goto done;
	local.position += 2;
	if (local.end + 8 < local.position) {
		result = SCE_JPEGENC_ERROR_INSUFFICIENT_BUFFER;
		goto done;
	}
	local.position[-2] = 0xff;
	local.position[-1] = JPEG_MARKER_EOI;
	local.tablesCompRatio = local.compRatio;
done:
	memcpy(context, &local, sizeof(local));
	if (result < 0)
		return result;
	return local.position - start;
}

/*
 * RPC 453's job: the input YCbCr (input, inputSize) encoded into the output
 * the context gives; the result in the context.
 */
void jpegEncoderJob(uint32_t argument, uint32_t input, uint32_t inputSize, uint32_t argument4)
{
	JpegEncoderContext *context = (JpegEncoderContext *)argument;
	VnzMemory inputMemory, outputMemory;
	int32_t result;

	vnzMemoryCreate(&inputMemory, input, inputSize);
	vnzMemoryCreate(&outputMemory, (uint32_t)context->position,
		context->end - context->position);
	if (vnzMemoryOpenProtected(&inputMemory) == 0 ||
		vnzMemoryOpenPrivate(&outputMemory) == 0) {
		result = ERROR_MEMORY;
		goto close;
	}
	result = setOutput(context, (uint8_t *)outputMemory.vaddrVnz, outputMemory.size);
	if (result >= 0)
		result = encode(context, (const uint8_t *)inputMemory.vaddrVnz);
close:
	if (outputMemory.vaddrVnz && vnzMemoryClosePrivate(&outputMemory) == 0 && result >= 0)
		result = ERROR_MEMORY;
	if (inputMemory.vaddrVnz && vnzMemoryCloseProtected(&inputMemory) == 0 && result >= 0)
		result = ERROR_MEMORY;
	context->result = result;
}
