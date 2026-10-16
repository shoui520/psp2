/*
 * The JPEG decoder (0x81db80-0x81f988): the headers, the stream and the
 * layout of the output. The scans themselves are decoded by the VLIW code.
 *
 * The stream may be given whole or in pieces (the split decoder): when its
 * input runs out the decoder keeps what it has not used at the start of the
 * buffer, waits for the ARM to write more after it (JPEG_INPUT_WAITING), and
 * then reads the buffer again from its start.
 */
#include <intrinsics.h>
#include "jpeg.h"

/* the standard Huffman tables (ITU T.81 K.3), a DHT segment's payload */
static const uint8_t defaultHuffmanTables[] = {
	/* luminance DC */
	0x00,
	0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
	/* chrominance DC */
	0x01,
	0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
	/* luminance AC */
	0x10,
	0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125,
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
	/* chrominance AC */
	0x11,
	0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119,
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
};

/*
 * The quantization tables scaled for the IDCT, (q * scale + 32) >> 6: a row
 * of 8 takes two of these halves. The 4x4 IDCT of a 1/2 downscale scales the
 * top left quarter by idctScale4x4 (rows 0 and 2 the first half, 1 and 3 the
 * second).
 */
static const uint8_t idctScaleHalves[8][2] = {
	{ 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 1 }, { 6, 7 }, { 4, 5 }, { 2, 3 },
};
const int16_t idctScale8x8[8][4] = {
	{ 4096, 5681, 5352, 4816 }, { 4096, 4816, 5352, 5681 },
	{ 5681, 7880, 7423, 6681 }, { 5681, 6681, 7423, 7880 },
	{ 5352, 7423, 6992, 6293 }, { 5352, 6293, 6992, 7423 },
	{ 4816, 6681, 6293, 5663 }, { 4816, 5663, 6293, 6681 },
};
const int16_t idctScale4x4[2][4] = {
	{ 4096, 5352, 4096, 5352 }, { 5352, 6992, 5352, 6992 },
};

/* the byte offset of each coefficient after the first, in zigzag order */
const uint8_t jpegZigzagOffset[64] = {
	2, 16, 32, 18, 4, 6, 20, 34, 48, 64, 50, 36, 22, 8, 10, 24,
	38, 52, 66, 80, 96, 82, 68, 54, 40, 26, 12, 14, 28, 42, 56, 70,
	84, 98, 112, 114, 100, 86, 72, 58, 44, 30, 46, 60, 74, 88, 102, 116,
	118, 104, 90, 76, 62, 78, 92, 106, 120, 122, 108, 94, 110, 124, 126, 126,
};

static int32_t nextMarker(JpegDecoder *decoder);
static int32_t parseSos(JpegDecoder *decoder);

/* the offset just past the SOI marker, or size */
static uint32_t findStartOfImage(const uint8_t *data, uint32_t size)
{
	uint32_t i;

	for (i = 0; i < size - 1; i += data[i + 1] == 0xff ? 2 : 1) {
		if (data[i + 1] == JPEG_MARKER_SOI)
			return data[i] == 0xff ? i + 2 : size;
	}
	return size;
}

/*
 * The planes of the region at the output (if given): each component's
 * pitch and rows, the steps between the blocks of an MCU, and from the end
 * of an MCU row to the next.
 */
static int32_t layoutPlanes(JpegDecoder *decoder, uint8_t *output)
{
	JpegComponent *luma = &decoder->component[0], *chroma;
	uint32_t shift = luma->scaleShift, blockSize = 8 >> shift;
	uint32_t colorSpace = decoder->colorSpace, mcuWidth;
	uint32_t width = (decoder->regionWidth << (JPEG_CS_HORIZONTAL(colorSpace) + 2)) >> shift;
	uint32_t rows = ((uint32_t)(decoder->regionEnd - decoder->regionY) <<
		(JPEG_CS_VERTICAL(colorSpace) + 2)) >> shift;
	uint32_t rowBytes, chromaWidth, chromaRows;
	int32_t i, k;

	if ((uint32_t)output & 0x3f)
		return SCE_JPEG_ERROR_INVALID_POINTER;
	if (decoder->flags & JPEG_DECODER_MJPEG_SAMPLING) {
		rows = (rows + 7) & ~7;
		width = (width + 15) & ~15;
	} else {
		mcuWidth = 4 << JPEG_CS_HORIZONTAL(colorSpace);
		width = (width + mcuWidth - 1) & ~(mcuWidth - 1);
	}
	luma->pitch = width;
	luma->rows = rows;
	rowBytes = blockSize * width;

	if (!decoder->progressive) {
		switch (colorSpace & JPEG_CS_SAMPLING_MASK) {
		case 5:		/* H1V1, H2V1, H4V1: side by side */
		case 9:
		case 13:
			for (i = 0; i < luma->blocks; i++)
				decoder->blockStep[i] = blockSize;
			break;
		case JPEG_CS_H2V2:
			decoder->blockStep[0] = blockSize;
			decoder->blockStep[1] = rowBytes - blockSize;
			decoder->blockStep[2] = blockSize;
			decoder->blockStep[3] = blockSize - rowBytes;
			break;
		default:	/* one above the other */
			for (i = 0; i < luma->blocks - 1; i++)
				decoder->blockStep[i] = rowBytes;
			decoder->blockStep[i] = blockSize - i * rowBytes;
			break;
		}
		luma->rowSkip = (blockSize << (JPEG_CS_VERTICAL(colorSpace) - 1)) * width -
			((decoder->regionWidth << (JPEG_CS_HORIZONTAL(colorSpace) + 2)) >> shift);
		i = luma->blocks;
	} else {
		i = 1;
		decoder->blockStep[0] = blockSize;
		luma->rowSkip = rowBytes -
			((decoder->regionWidth << (JPEG_CS_HORIZONTAL(colorSpace) + 2)) >> shift);
	}

	chromaWidth = decoder->regionWidth * blockSize;
	chromaRows = (decoder->regionEnd - decoder->regionY) * blockSize;
	if (decoder->flags & JPEG_DECODER_MJPEG_SAMPLING) {
		chromaWidth = (chromaWidth + 15) & ~15;
		chromaRows = luma->rows >> (JPEG_CS_VERTICAL(colorSpace) - 1);
	} else {
		chromaWidth = (chromaWidth + 7) & ~7;
	}
	for (k = 1; k < JPEG_COMPONENTS; k++, i++) {
		chroma = &decoder->component[k];
		decoder->blockStep[i] = blockSize;
		chroma->rows = chromaRows;
		chroma->pitch = chromaWidth;
		chroma->rowSkip = (8 >> chroma->scaleShift) * chromaWidth -
			decoder->regionWidth * blockSize;
	}

	if (output) {
		for (k = 0; k < JPEG_COMPONENTS; k++) {
			decoder->component[k].plane = output;
			output += decoder->component[k].pitch * decoder->component[k].rows;
		}
	}
	return 0;
}

/*
 * The progressive coefficients, block after block (128 bytes each) for each
 * component in turn at coef (if given): the steps between the blocks of an
 * MCU and the size needed.
 */
static int32_t layoutCoefs(JpegDecoder *decoder, int16_t *coef, uint32_t *size)
{
	uint32_t colorSpace = decoder->colorSpace, blockColumns, mcus;
	uint64_t bytes, total;
	JpegComponent *component;
	int32_t i, j, k;

	if (coef) {
		blockColumns = decoder->mcuColumns << (JPEG_CS_HORIZONTAL(colorSpace) - 1);
		if (decoder->mcuBlock[1] == NULL) {
			/* one component: block by block */
			component = decoder->mcuBlock[0];
			if (component->blocks == 1) {
				decoder->scanRows = decoder->mcuRows;
				decoder->scanColumns = decoder->mcuColumns;
				component->coefRowSkip = 0;
			} else {
				decoder->scanRows = (decoder->height + 7) >> 3;
				if (decoder->scanPhase == JPEG_SCAN_OUTPUT) {
					decoder->scanColumns = blockColumns;
					component->coefRowSkip = 0;
				} else {
					decoder->scanColumns = (decoder->width + 7) >> 3;
					component->coefRowSkip = (blockColumns - decoder->scanColumns) *
						JPEG_BLOCK_COEF_SIZE;
				}
			}
			decoder->coefStep[0] = JPEG_BLOCK_COEF_SIZE;
		} else {
			decoder->scanColumns = decoder->mcuColumns;
			decoder->scanRows = decoder->mcuRows;
			for (i = 0; (component = decoder->mcuBlock[i]) != NULL; ) {
				if (component != &decoder->component[0]) {
					decoder->coefStep[i++] = JPEG_BLOCK_COEF_SIZE;
					component->coefRowSkip = 0;
					continue;
				}
				switch (colorSpace & JPEG_CS_SAMPLING_MASK) {
				case 5:
				case 9:
				case 13:
					for (j = 0; j < component->blocks; j++)
						decoder->coefStep[i + j] = JPEG_BLOCK_COEF_SIZE;
					break;
				case 6:
				case 7:
					for (j = 0; j < component->blocks - 1; j++)
						decoder->coefStep[i + j] = blockColumns * JPEG_BLOCK_COEF_SIZE;
					decoder->coefStep[i + j] = JPEG_BLOCK_COEF_SIZE -
						j * JPEG_BLOCK_COEF_SIZE * blockColumns;
					break;
				case JPEG_CS_H2V2:
					decoder->coefStep[i] = JPEG_BLOCK_COEF_SIZE;
					decoder->coefStep[i + 1] = blockColumns * JPEG_BLOCK_COEF_SIZE -
						JPEG_BLOCK_COEF_SIZE;
					decoder->coefStep[i + 2] = JPEG_BLOCK_COEF_SIZE;
					decoder->coefStep[i + 3] = JPEG_BLOCK_COEF_SIZE -
						blockColumns * JPEG_BLOCK_COEF_SIZE;
					break;
				default:
					i += component->blocks;
					continue;
				}
				decoder->component[0].coefRowSkip = 0;
				i += component->blocks;
			}
		}
	}

	mcus = (uint32_t)decoder->mcuColumns * decoder->mcuRows;
	bytes = (uint64_t)mcus * JPEG_BLOCK_COEF_SIZE;
	if (bytes >> 31)
		return SCE_JPEG_ERROR_UNSUPPORT_IMAGE_SIZE;
	total = 0;
	for (k = 0; k < JPEG_COMPONENTS; k++) {
		component = &decoder->component[k];
		if (coef)
			component->coef = coef;
		if ((bytes * component->blocks) >> 31)
			return SCE_JPEG_ERROR_UNSUPPORT_IMAGE_SIZE;
		coef = (int16_t *)((uint8_t *)coef + (uint32_t)(bytes * component->blocks));
		total += (uint32_t)(bytes * component->blocks);
	}
	total += 32;
	if (total >> 31)
		return SCE_JPEG_ERROR_UNSUPPORT_IMAGE_SIZE;
	if (size)
		*size = total;
	return 0;
}

/* the rest of a long segment skipped, or all the input */
static int32_t skipInput(JpegDecoder *decoder)
{
	uint32_t left = decoder->streamEnd - decoder->streamPosition;

	if (decoder->skipLength < left) {
		decoder->streamPosition += decoder->skipLength;
		decoder->skipLength = 0;
		return 0;
	}
	decoder->skipLength -= left;
	decoder->streamPosition = decoder->streamBase;
	decoder->inputState |= JPEG_INPUT_WAITING;
	return SCE_JPEG_ERROR_INPUT_SUSPENDED;
}

/* what is left from here moved to the start; more input goes after it */
static void keepInput(JpegDecoder *decoder, const uint8_t *from)
{
	uint8_t *to = (uint8_t *)decoder->streamBase;
	uint8_t *end = to + (decoder->streamEnd - from);

	while (to != end)
		*to++ = *from++;
	decoder->streamPosition = end;
}

/*
 * A split decoder's error at the end of its input: the decoder waits for
 * more, from the marker that did not fit (a long APPn or COM segment is
 * skipped instead), unless the buffer is too small for it.
 */
static int32_t suspendInput(JpegDecoder *decoder, int32_t error)
{
	const uint8_t *position = decoder->streamPosition;
	uint32_t marker, length;
	uint8_t state;

	if (error == SCE_JPEG_ERROR_INPUT_SUSPENDED) {
		if (position <= decoder->streamBase)
			return SCE_JPEG_ERROR_INPUT_DATA_TOO_BIG;
		if (decoder->streamEnd < position)
			return error;
	} else if (error == SCE_JPEG_ERROR_BAD_MARKER_LENGTH) {
		state = decoder->inputState;
		if (state != JPEG_INPUT_HEADER && state != JPEG_INPUT_TRAILER &&
			state != JPEG_INPUT_DECODE)
			return error;
		if (decoder->streamBase < position) {
			marker = 0;
			length = 0;
			if (position + 1 < decoder->streamEnd) {
				marker = position[-1];
				length = position[0] << 8 | position[1];
			}
			if (marker >= 0xe0 && marker < 0xff && length) {
				decoder->streamPosition = decoder->streamBase;
				decoder->skipLength = length - (decoder->streamEnd - position);
				goto suspended;
			}
		}
		position -= 2;
		if (position <= decoder->streamBase)
			return SCE_JPEG_ERROR_INPUT_DATA_TOO_BIG;
		if (decoder->streamEnd < position)
			return error;
	} else {
		return error;
	}
	keepInput(decoder, position);
suspended:
	decoder->inputState |= JPEG_INPUT_WAITING;
	return SCE_JPEG_ERROR_INPUT_SUSPENDED;
}

/* the image done: the EOI must follow */
static int32_t expectEndOfImage(JpegDecoder *decoder)
{
	int32_t result = nextMarker(decoder);

	if (result < 0) {
		if (decoder->inputState == JPEG_INPUT_WHOLE)
			return result;
		decoder->inputState = JPEG_INPUT_TRAILER;
		return suspendInput(decoder, result);
	}
	if (result == JPEG_MARKER_EOI)
		return 0;
	decoder->mcuRow = -1;
	return SCE_JPEG_ERROR_EOI_EXPECTED;
}

/* a progressive image's next scan header */
static int32_t readScanHeader(JpegDecoder *decoder)
{
	int32_t result;

	decoder->scanPhase = JPEG_SCAN_HEADER;
	result = nextMarker(decoder);
	if (result < 0) {
		if (decoder->inputState)
			return suspendInput(decoder, result);
		return result;
	}
	if (result != JPEG_MARKER_SOS)
		return SCE_JPEG_ERROR_UNEXPECTED_MARKER;
	result = parseSos(decoder);
	if (result < 0) {
		if (decoder->inputState) {
			result = suspendInput(decoder, result);
			if (result == SCE_JPEG_ERROR_INPUT_SUSPENDED)
				return result;
		}
		decoder->mcuRow = -1;
		return result;
	}
	layoutCoefs(decoder, decoder->coefBuffer, NULL);
	decoder->scanPhase = JPEG_SCAN_DATA;
	return 1;
}

/* the coefficients of one component made pixels, in units of its blocks */
static void prepareOutput(JpegDecoder *decoder)
{
	int32_t index = -decoder->coefsDone;
	uint32_t colorSpace = decoder->colorSpace;
	uint32_t vertical = JPEG_CS_VERTICAL(colorSpace) - 1;
	uint32_t horizontal = JPEG_CS_HORIZONTAL(colorSpace) - 1;

	decoder->mcuBlock[1] = NULL;
	decoder->mcuBlock[0] = &decoder->component[index];
	if (index == 0) {
		decoder->blockColumns = decoder->mcuColumns << horizontal;
		decoder->regionX <<= horizontal;
		decoder->regionY <<= vertical;
		decoder->regionWidth <<= horizontal;
		decoder->regionEnd <<= vertical;
	}
	if (index == 1) {
		decoder->blockColumns = decoder->mcuColumns;
		decoder->regionX >>= horizontal;
		decoder->regionWidth >>= horizontal;
		decoder->regionEnd >>= vertical;
		decoder->regionY >>= vertical;
	}
	decoder->scanRow = jpegOutputComponentRow;
}

/*
 * A progressive image's next scan; when every coefficient is complete, the
 * output, one component after another (coefsDone counting down from 0).
 */
static int32_t nextScan(JpegDecoder *decoder)
{
	int32_t result;

	if (decoder->coefsDone < decoder->coefsTotal) {
		decoder->mcuRow = 0;
		decoder->mcuColumn = 0;
		result = readScanHeader(decoder);
		if (result != SCE_JPEG_ERROR_UNEXPECTED_MARKER)
			return result;
		/* no more scans: output what there is */
		decoder->coefsDone = decoder->coefsTotal;
		decoder->streamPosition -= 2;
	}
	if (decoder->coefsTotal == decoder->coefsDone) {
		decoder->scanPhase = JPEG_SCAN_OUTPUT;
		decoder->scanCoefs = -1;
		decoder->coefsDone = 0;
		decoder->coefsTotal = -decoder->coefsTotal / 64;
	}
	if (decoder->coefsTotal < decoder->coefsDone) {
		prepareOutput(decoder);
		decoder->mcuRow = 0;
		decoder->mcuColumn = -decoder->regionX;
		layoutCoefs(decoder, decoder->coefBuffer, NULL);
		return 1;
	}
	if (decoder->scanStatus != JPEG_SCAN_FAILED)
		return expectEndOfImage(decoder);
	return SCE_JPEG_ERROR_DECODE_ERROR;
}

/* a row of MCUs (of blocks, progressive): 1 while more follow, 0 at the end */
int32_t jpegDecoderStep(void *work)
{
	JpegDecoder *decoder = JPEG_DECODER(work);
	uint8_t state = decoder->inputState;
	JpegComponent *component;
	int32_t result, row, k;

	if (state >= JPEG_INPUT_WAITING)
		return SCE_JPEG_ERROR_INVALID_STATE;
	if (state >= JPEG_INPUT_TRAILER) {
		if (state == JPEG_INPUT_TRAILER_END)
			decoder->inputState = JPEG_INPUT_WHOLE;
		return expectEndOfImage(decoder);
	}

	if (!decoder->progressive) {
		if ((uint32_t)decoder->mcuRow >= decoder->regionEnd ||
			decoder->streamPosition >= decoder->streamEnd)
			return SCE_JPEG_ERROR_CANNOT_CONTINUE;
		result = jpegDecodeMcuRow(decoder);
		if (result < 0)
			goto failed;
		row = decoder->mcuRow + 1;
		decoder->mcuRow = row;
		if (row == decoder->mcuRows)
			return expectEndOfImage(decoder);
		for (k = 0; k < JPEG_COMPONENTS; k++)
			decoder->component[k].plane += decoder->component[k].rowSkip;
		return 1;
	}

	if ((uint32_t)decoder->mcuRow >= decoder->scanRows ||
		decoder->streamPosition >= decoder->streamEnd)
		return SCE_JPEG_ERROR_CANNOT_CONTINUE;
	if (decoder->skipLength) {
		result = skipInput(decoder);
		if (result < 0)
			return result;
	}
	if (decoder->scanPhase == JPEG_SCAN_HEADER)
		return nextScan(decoder);
	result = decoder->scanRow(decoder);
	if (result < 0) {
		if (result != SCE_JPEG_ERROR_DECODE_ERROR)
			goto failed;
		/* the scan given up: the next one, the image left as it is */
		decoder->coefsDone = decoder->coefsTotal - decoder->scanCoefs;
		decoder->scanStatus = JPEG_SCAN_FAILED;
		decoder->mcuRow = decoder->scanRows - 1;
	}
	row = decoder->mcuRow + 1;
	decoder->mcuRow = row;
	if (row == decoder->scanRows) {
		decoder->coefsDone += decoder->scanCoefs;
		return nextScan(decoder);
	}
	for (k = 0; k < JPEG_COMPONENTS; k++) {
		component = &decoder->component[k];
		component->coef = (int16_t *)((uint8_t *)component->coef + component->coefRowSkip);
	}
	if (decoder->scanPhase == JPEG_SCAN_OUTPUT) {
		component = &decoder->component[-decoder->coefsDone];
		component->plane += component->rowSkip;
	}
	return 1;

failed:
	if (decoder->inputState == JPEG_INPUT_WHOLE)
		return result;
	return suspendInput(decoder, result);
}

/*
 * The decoder started on a stream: the headers up to the first scan read.
 * Returns the size of the YCbCr planes, with the coefficients' in coefSize.
 */
int32_t jpegDecoderInit(void *work, const uint8_t *stream, uint32_t size, uint32_t downscale,
	uint32_t flags, uint32_t partial, uint32_t *coefSize)
{
	JpegDecoder *decoder = JPEG_DECODER(work);
	uint32_t offset;
	int32_t k;

	decoder->progressive = 0;
	decoder->restartInterval = 0;
	decoder->restartsLeft = 0;
	decoder->coefBuffer = NULL;
	for (k = 0; k < JPEG_COMPONENTS; k++) {
		decoder->huffmanTable[k][JPEG_HUFFMAN_DC].defined = 0;
		decoder->huffmanTable[k][JPEG_HUFFMAN_AC].defined = 0;
	}
	for (k = 0; k < JPEG_COMPONENTS; k++)
		decoder->component[k].blocks = 0;
	decoder->inputState = partial == 1 ? JPEG_INPUT_SPLIT : JPEG_INPUT_WHOLE;

	offset = findStartOfImage(stream, size);
	if (offset == size)
		return SCE_JPEG_ERROR_NO_SOI;
	decoder->flags = flags;
	if ((downscale & (downscale - 1)) || (int32_t)downscale < 1 || (int32_t)downscale >= 9)
		return SCE_JPEG_ERROR_INVALID_DECODE_MODE;
	decoder->streamBase = stream;
	decoder->downscale = downscale;
	decoder->streamPosition = stream + offset;
	decoder->streamEnd = stream + size;
	decoder->skipLength = 0;
	decoder->colorSpace = 0;
	if (decoder->inputState)
		decoder->inputState = JPEG_INPUT_HEADER;
	decoder->scanPhase = JPEG_SCAN_NONE;
	decoder->scanStatus = 0;
	decoder->coefsTotal = 0;
	decoder->coefsDone = 0;
	decoder->scanCoefs = 0;
	return jpegDecoderContinue(decoder, coefSize);
}

/* the headers read on to the first scan, again after more input */
int32_t jpegDecoderContinue(void *work, uint32_t *coefSize)
{
	JpegDecoder *decoder = JPEG_DECODER(work);
	uint32_t colorSpace, sampling, shift;
	uint64_t total;
	int32_t result, k;

	if ((uint8_t)decoder->inputState >= JPEG_INPUT_WAITING)
		return SCE_JPEG_ERROR_INVALID_STATE;
	if (decoder->skipLength) {
		result = skipInput(decoder);
		if (result < 0)
			return result;
	}
	result = nextMarker(decoder);
	if (result < 0) {
		if (decoder->inputState == JPEG_INPUT_WHOLE)
			return result;
		return suspendInput(decoder, result);
	}
	if (result != JPEG_MARKER_SOS) {
		decoder->mcuRow = -1;
		return SCE_JPEG_ERROR_UNEXPECTED_MARKER;
	}

	/* the whole image */
	decoder->regionX = 0;
	decoder->regionY = 0;
	decoder->mcuRow = 0;
	decoder->mcuColumn = 0;
	decoder->regionWidth = decoder->mcuColumns;
	decoder->regionEnd = decoder->mcuRows;
	result = parseSos(decoder);
	if (result < 0) {
		if (decoder->inputState) {
			result = suspendInput(decoder, result);
			if (result == SCE_JPEG_ERROR_INPUT_SUSPENDED)
				return result;
		}
		decoder->mcuRow = -1;
		return result;
	}

	colorSpace = decoder->colorSpace;
	decoder->frameColorSpace = colorSpace;
	shift = 31 - __builtin_clz(decoder->downscale);
	sampling = colorSpace & JPEG_CS_SAMPLING_MASK;
	if (decoder->flags & JPEG_DECODER_MJPEG_SAMPLING) {
		if ((colorSpace & JPEG_CS_KIND_MASK) != JPEG_CS_YCBCR)
			return SCE_JPEG_ERROR_UNSUPPORT_COLORSPACE;
		if (sampling != JPEG_CS_H2V2 && sampling != JPEG_CS_H2V1)
			return SCE_JPEG_ERROR_UNSUPPORT_SAMPLING;
	}
	if (!(sampling & 3) || !(sampling & 0xc) || sampling == 14 || sampling == 11 ||
		sampling == 15)
		return SCE_JPEG_ERROR_UNSUPPORT_SAMPLING;
	for (k = 0; k < JPEG_COMPONENTS; k++) {
		decoder->component[k].scale = decoder->downscale;
		decoder->component[k].scaleShift = shift;
	}

	result = layoutPlanes(decoder, NULL);
	if (result < 0)
		return result;
	if (decoder->progressive) {
		result = layoutCoefs(decoder, NULL, coefSize);
		if (result < 0)
			return result;
	} else {
		*coefSize = 0;
	}
	total = 0;
	for (k = 0; k < JPEG_COMPONENTS && decoder->component[k].blocks; k++)
		total += decoder->component[k].pitch * decoder->component[k].rows;
	if (total >= 0x80000000)
		return SCE_JPEG_ERROR_UNSUPPORT_IMAGE_SIZE;
	if (decoder->inputState)
		decoder->inputState = JPEG_INPUT_DECODE;
	if (decoder->progressive)
		decoder->scanPhase = JPEG_SCAN_DATA;
	return total;
}

/*
 * The output given (and the coefficients, progressive), with a region in
 * MCUs from x, y up to width, height (all 0: the whole image).
 */
int32_t jpegDecoderStart(void *work, uint8_t *output, int32_t x, int32_t y, int32_t width,
	int32_t height, int16_t *coef)
{
	JpegDecoder *decoder = JPEG_DECODER(work);
	int32_t result;

	if (output == NULL)
		return SCE_JPEG_ERROR_INVALID_POINTER;
	if (decoder->progressive) {
		if (coef == NULL || ((uint32_t)coef & 0x3f))
			return SCE_JPEG_ERROR_INVALID_POINTER;
		layoutCoefs(decoder, coef, NULL);
		decoder->coefBuffer = coef;
	}
	if (decoder->mcuRow != 0 || (decoder->inputState != JPEG_INPUT_WHOLE &&
		decoder->inputState != JPEG_INPUT_DECODE))
		return SCE_JPEG_ERROR_INVALID_STATE;
	if (x | y | width | height) {
		if (x < 0 || y < 0 || decoder->mcuColumns < width || decoder->mcuRows < height ||
			x >= width || y >= height)
			return SCE_JPEG_ERROR_INVALID_REGION;
		decoder->regionY = y;
		decoder->regionX = x;
		decoder->regionWidth = width - x;
		decoder->regionEnd = height;
		decoder->mcuColumn = decoder->progressive ? 0 : -x;
	}
	result = layoutPlanes(decoder, output);
	return result < 0 ? result : 0;
}

/* sceJpegDecodeMJpegYCbCr: the whole image, the decoder on the stack */
int32_t jpegDecodeMJpegYCbCr(const uint8_t *pJpeg, uint32_t isize, uint8_t *pYCbCr,
	uint32_t osize, uint32_t flags, uint32_t downscale, int32_t *colorSpace, void *pCoefBuffer,
	uint32_t coefBufferSize)
{
	uint8_t work[sizeof(JpegDecoder) + JPEG_DECODER_ALIGN];
	JpegDecoder *decoder = JPEG_DECODER(work);
	uint32_t coefSize;
	int32_t result;

	result = jpegDecoderInit(decoder, pJpeg, isize, downscale, flags, 0, &coefSize);
	if (result < 0)
		return result;
	if (osize < (uint32_t)result)
		return SCE_JPEG_ERROR_MEMORY_SIZE;
	result = layoutPlanes(decoder, pYCbCr);
	if (result < 0)
		return result;
	if (decoder->progressive) {
		if (pCoefBuffer == NULL || ((uint32_t)pCoefBuffer & 0x3f))
			return SCE_JPEG_ERROR_INVALID_POINTER;
		if (coefBufferSize < coefSize)
			return SCE_JPEG_ERROR_MEMORY_SIZE;
		layoutCoefs(decoder, pCoefBuffer, NULL);
		decoder->coefBuffer = pCoefBuffer;
	}
	do
		result = jpegDecoderStep(decoder);
	while (result >= 1);
	if (result)
		return result;
	result = jpegDecoderFinish(decoder);
	if (colorSpace)
		*colorSpace = decoder->colorSpace;
	return result;
}

/* where the next input goes, and how much fits */
int32_t jpegDecoderInputPosition(void *work, uint32_t *position, uint32_t *size)
{
	JpegDecoder *decoder = JPEG_DECODER(work);

	if (position == NULL || size == NULL)
		return SCE_JPEG_ERROR_INVALID_POINTER;
	if ((uint8_t)decoder->inputState < JPEG_INPUT_WAITING)
		return SCE_JPEG_ERROR_INVALID_STATE;
	*position = (uint32_t)decoder->streamPosition;
	*size = decoder->streamEnd - decoder->streamPosition;
	return 0;
}

/* size bytes written there: the stream read again from its start */
int32_t jpegDecoderSetInput(void *work, uint32_t size, uint32_t isEndOfStream)
{
	JpegDecoder *decoder = JPEG_DECODER(work);
	const uint8_t *written;

	if ((size | isEndOfStream) == 0 ||
		(uint32_t)(decoder->streamEnd - decoder->streamPosition) < size)
		return SCE_JPEG_ERROR_INVALID_DATA_SIZE;
	if ((uint8_t)decoder->inputState < JPEG_INPUT_WAITING)
		return SCE_JPEG_ERROR_INVALID_STATE;
	written = decoder->streamPosition;
	decoder->streamPosition = decoder->streamBase;
	decoder->streamEnd = written + size;
	decoder->inputState &= ~JPEG_INPUT_WAITING;
	if (isEndOfStream == 1)
		decoder->inputState = decoder->inputState == JPEG_INPUT_TRAILER ?
			JPEG_INPUT_TRAILER_END : JPEG_INPUT_WHOLE;
	return 0;
}

/* sceJpegGetOutputInfo: the headers read, the decoder on the stack */
int32_t jpegGetOutputInfo(const uint8_t *pJpeg, uint32_t isize, uint32_t flags,
	uint32_t downscale, int32_t outputFormat, SceJpegOutputInfo *pOutputInfo)
{
	uint8_t work[sizeof(JpegDecoder) + JPEG_DECODER_ALIGN];
	JpegDecoder *decoder = JPEG_DECODER(work);
	uint32_t coefSize;
	int32_t result;

	result = jpegDecoderInit(decoder, pJpeg, isize, downscale, flags, 0, &coefSize);
	if (result < 0) {
		pOutputInfo->colorSpace = decoder->colorSpace;
		return result;
	}
	return jpegDecoderOutputInfo(decoder, outputFormat, result, coefSize, pOutputInfo);
}

/*
 * The buffers the image needs: YCbCr planes (outputFormat < 0), or RGBA of
 * the luma plane's size with the planes as the temporary buffer.
 */
int32_t jpegDecoderOutputInfo(void *work, int32_t outputFormat, int32_t outputSize,
	uint32_t coefSize, SceJpegOutputInfo *pOutputInfo)
{
	JpegDecoder *decoder = JPEG_DECODER(work);
	JpegComponent *component;
	uint64_t size;
	int32_t k;

	pOutputInfo->colorSpace = decoder->colorSpace;
	pOutputInfo->imageWidth = decoder->width;
	pOutputInfo->imageHeight = decoder->height;
	pOutputInfo->coefBufferSize = coefSize;
	if (outputFormat < 0) {
		pOutputInfo->outputBufferSize = outputSize;
		pOutputInfo->tempBufferSize = 0;
		for (k = 0; k < JPEG_COMPONENTS; k++) {
			component = &decoder->component[k];
			if (component->blocks == 0) {
				pOutputInfo->pitch[k].x = 0;
				pOutputInfo->pitch[k].y = 0;
			} else {
				pOutputInfo->pitch[k].x = component->pitch;
				pOutputInfo->pitch[k].y = component->rows;
			}
		}
		return 0;
	}

	component = &decoder->component[0];
	size = (uint64_t)(component->pitch * 4) * component->rows;
	pOutputInfo->tempBufferSize = outputSize;
	pOutputInfo->pitch[0].x = component->pitch * 4;
	pOutputInfo->pitch[0].y = component->rows;
	pOutputInfo->outputBufferSize = size;
	for (k = 1; k < JPEG_COMPONENTS; k++) {
		pOutputInfo->pitch[k].x = 0;
		pOutputInfo->pitch[k].y = 0;
	}
	if (pOutputInfo->outputBufferSize != size)
		return SCE_JPEG_ERROR_UNSUPPORT_IMAGE_SIZE;
	return 0;
}

/* the luma plane's size, pitch << 16 | rows */
int32_t jpegDecoderFinish(void *work)
{
	JpegDecoder *decoder = JPEG_DECODER(work);

	return (decoder->component[0].pitch << 16) + decoder->component[0].rows;
}

/*
 * For the scan decoders: up to three more bytes of entropy-coded data into
 * bits (the first in bits 8-15, the second 0-7, the third 24-31), the bytes
 * stuffed after 0xff skipped; a marker stops it, left unread.
 */
uint32_t jpegStreamFetch(const uint8_t **position, const uint8_t *end, uint32_t bits)
{
	const uint8_t *p = *position;
	uint32_t byte, next;

	end--;
	if (p >= end)
		return bits;
	byte = *p++;
	bits = (bits & 0xffff00ff) | byte << 8;
	next = *p;
	if (byte == 0xff) {
		if (next)
			goto marker;
		if (p < end)
			p++;
		next = *p;
	}
	if (p < end)
		p++;
	byte = next;
	next = *p;
	bits = (bits & ~0xff) | byte;
	if (byte == 0xff) {
		if (next)
			goto marker;
		if (p < end)
			p++;
		next = *p;
	}
	if (p < end)
		p++;
	byte = next;
	next = *p;
	bits = (bits & 0xffffff) | byte << 24;
	if (byte == 0xff) {
		if (next)
			goto marker;
		if (p < end)
			p++;
	}
	goto done;
marker:
	p--;
done:
	*position = p;
	return bits;
}

/*
 * Entropy-coded data from p: the predictors and the EOB run reset, and up
 * to four bytes in the bit buffer (bitCount 8 less the bits loaded, so above
 * 0 when there were none).
 */
static void resetBits(JpegDecoder *decoder, const uint8_t *p)
{
	const uint8_t *end = decoder->streamEnd;
	uint32_t bits = 0, byte;
	int32_t shift = 24, i;

	for (i = 0; i < JPEG_COMPONENTS; i++)
		decoder->component[i].dcPredictor = 0;
	decoder->eobRun = 0;
	for (i = 0; i < 4; i++) {
		if (p >= end)
			break;
		byte = *p;
		if (byte == 0xff) {
			p++;
			if (p >= decoder->streamEnd || *p) {
				p--;
				break;
			}
		}
		bits |= byte << shift;
		shift -= 8;
		p++;
	}
	decoder->bitBuffer = bits;
	decoder->streamPosition = p;
	decoder->bitCount = shift - 16;
}

/*
 * For the scan decoders: the restart marker due, searched for in the next
 * 1536 bytes. A progressive image's last MCU may go without it.
 */
int32_t jpegDecoderRestart(JpegDecoder *decoder, int32_t column)
{
	const uint8_t *p = decoder->streamPosition;
	int32_t tries = 1536;
	uint8_t marker;

	for (;;) {
		if (p + 1 >= decoder->streamEnd)
			break;
		if (p[0] == 0xff) {
			marker = p[1];
			if (marker == decoder->restartMarker) {
				resetBits(decoder, p + 2);
				if (decoder->inputState && (decoder->bitCount >= 1 ||
					decoder->streamPosition >= decoder->streamEnd))
					goto suspended;
				decoder->restartMarker = (marker + 1) & 0xf7;
				decoder->restartsLeft = decoder->restartInterval;
				return 0;
			}
			if (marker == JPEG_MARKER_EOI) {
				decoder->streamPosition = p;
				return JPEG_MARKER_EOI;
			}
			if (!decoder->progressive || decoder->mcuRow + 1 < decoder->scanRows ||
				column + 1 < decoder->scanColumns)
				break;
			return 0;
		}
		p++;
		if (--tries < 1)
			break;
	}
	if (decoder->inputState == JPEG_INPUT_WHOLE)
		return SCE_JPEG_ERROR_BAD_RESTART_MARKER;
suspended:
	decoder->restartsLeft = 1;
	return SCE_JPEG_ERROR_INPUT_SUSPENDED;
}

/* for the scan decoders: the state before an MCU */
void jpegDecoderSaveState(JpegDecoder *decoder, JpegDecoderSave *save)
{
	JpegComponent *component;
	int32_t k;

	save->streamPosition = decoder->streamPosition;
	save->bitBuffer = decoder->bitBuffer;
	save->bitCount = decoder->bitCount;
	save->eobRun = decoder->eobRun;
	for (k = 0; k < JPEG_COMPONENTS; k++) {
		component = &decoder->component[k];
		save->component[k].dcPredictor = component->dcPredictor;
		save->component[k].plane = component->plane;
		save->component[k].coef = component->coef;
	}
}

static void restoreState(JpegDecoder *decoder, const JpegDecoderSave *save)
{
	JpegComponent *component;
	int32_t k;

	decoder->streamPosition = save->streamPosition;
	decoder->bitBuffer = save->bitBuffer;
	decoder->bitCount = save->bitCount;
	decoder->eobRun = save->eobRun;
	for (k = 0; k < JPEG_COMPONENTS; k++) {
		component = &decoder->component[k];
		component->dcPredictor = save->component[k].dcPredictor;
		component->plane = save->component[k].plane;
		component->coef = save->component[k].coef;
	}
}

/*
 * For the scan decoders, an MCU's result: out of input, a split decoder goes
 * back to the MCU's start (column) to wait; any other error stops the image.
 */
int32_t jpegDecoderMcuResult(JpegDecoder *decoder, int32_t result, int32_t column,
	JpegDecoderSave *save)
{
	if (result >= 0)
		return result;
	if (result == SCE_JPEG_ERROR_INPUT_SUSPENDED) {
		if (decoder->inputState) {
			restoreState(decoder, save);
			decoder->mcuColumn = column;
			return result;
		}
		result = SCE_JPEG_ERROR_DECODE_ERROR;
	}
	decoder->mcuRow = -1;
	return result;
}

/* for the scan decoders: a block with only its DC coefficient, as a pixel */
void jpegDcOnlyPixel(const int16_t *coef, const int16_t *quant, uint8_t *pixel)
{
	int32_t value = (coef[0] * quant[0] + 1024) >> 3;

	*pixel = value < 0 ? 0 : value > 255 ? 255 : value;
}

/* DHT: the tables of the segment (length bytes) */
static int32_t parseDht(JpegDecoder *decoder, int32_t length)
{
	const uint8_t *p = decoder->streamPosition;
	int32_t total, i, codeLength, lastLength, size, bits, l;
	uint32_t code, fill, info, isAc, symbol;
	int8_t entryLength, entryValue;
	JpegHuffmanTable *table;
	uint8_t counts[16];		/* codes up to each length */

	while (length >= 1) {
		info = *p++;
		if ((info & 0xf) >= JPEG_COMPONENTS)
			return SCE_JPEG_ERROR_BAD_DHT_INDEX;
		table = &decoder->huffmanTable[info & 0xf][(info >> 4) & 1];
		total = 0;
		for (i = 0; i < 16; i++) {
			total += *p++;
			counts[i] = total;
		}
		if (total >= 256)
			return SCE_JPEG_ERROR_BAD_DHT_COUNTS;

		codeLength = 1;
		lastLength = 8;
		code = 0;
		fill = 0;
		isAc = info & 0x10;
		for (i = 0; i < total; i++) {
			symbol = *p++;
			while (counts[codeLength - 1] <= i)
				codeLength++;
			size = symbol & 0xf;
			if (codeLength < 9) {
				fill = (256 >> codeLength) + code;
				if (fill >= 257)
					return SCE_JPEG_ERROR_BAD_DHT_COUNTS;
				if (size && codeLength + size < 9) {
					/* the code and its value's bits both in the lookup */
					entryLength = codeLength + size;
					if (isAc)
						entryLength = ((symbol >> 4) & 0xf) - entryLength * 16;
					do {
						bits = (code << (codeLength + 24)) >> (32 - size);
						if (bits < 1 << (size - 1))
							bits -= (1 << size) - 1;
						table->lookupValue[code] = bits;
						table->lookupLength[code] = entryLength;
					} while (++code < fill);
				} else {
					entryLength = isAc ? codeLength : -codeLength;
					entryValue = isAc ? (int8_t)symbol : -size;
					do {
						table->lookupValue[code] = entryValue;
						table->lookupLength[code] = entryLength;
					} while (++code < fill);
				}
				fill = code;
			} else {
				table->longValue[246 - i + counts[7]] = isAc ? (int8_t)symbol : -size;
				if (lastLength < codeLength) {
					if (lastLength >= 9) {
						for (l = lastLength; l < codeLength; l++)
							table->limit[16 - l] = code << (16 - lastLength);
						table->valueOffset[16 - lastLength] = code - i + counts[7] - 1;
					} else if (codeLength >= 10) {
						for (l = 9; l < codeLength; l++)
							table->limit[16 - l] = 0;
					}
					code <<= codeLength - lastLength;
					lastLength = codeLength;
				}
				code++;
			}
		}

		if (lastLength != 8) {
			do
				table->lookupLength[fill] = isAc ? JPEG_HUFFMAN_LONG_AC : 0;
			while (++fill < 256);
			if ((int32_t)(code - i) < 0)
				return SCE_JPEG_ERROR_BAD_DHT_COUNTS;
			table->valueOffset[16 - codeLength] = code - i + counts[7] - 1;
			code <<= 16 - lastLength;
			for (l = lastLength; l < 16; l++)
				table->limit[16 - l] = code;
			table->limit[0] = 0;
		} else {
			/* the codes left unused look up the last one */
			do {
				table->lookupLength[fill] = table->lookupLength[fill - 1];
				table->lookupValue[fill] = table->lookupValue[fill - 1];
			} while (++fill < 256);
		}
		total += 17;
		table->defined = 1;
		length -= total;
	}
	if (length < 0)
		return SCE_JPEG_ERROR_BAD_DHT_MARKER;
	return 0;
}

/* DQT: the tables of the segment, scaled for the IDCT of the downscale */
static int32_t parseDqt(JpegDecoder *decoder, int32_t length)
{
	const uint8_t *p = decoder->streamPosition;
	uint32_t info, wide, value, k;
	uint16_t *table;
	cp_v4hi *row, *half;
	cp_v4hi round = mep_cpmovi_h(32);
	const cp_v4hi *scale8 = (const cp_v4hi *)idctScale8x8;
	const cp_v4hi *scale4 = (const cp_v4hi *)idctScale4x4;

	while (length >= 1) {
		info = *p++;
		wide = (info & 0x10) << 2;
		if ((info & 0xf) >= JPEG_COMPONENTS)
			return SCE_JPEG_ERROR_BAD_DQT_INDEX;
		table = decoder->quantTable[info & 0xf];
		for (k = 0; k < 64; k++) {
			value = *p++;
			if (wide)
				value = (value << 8) + *p++;
			*(uint16_t *)((uint8_t *)table + (k ? jpegZigzagOffset[k - 1] : 0)) = value;
		}

		if (decoder->downscale == 1) {
			row = (cp_v4hi *)table;
			half = row + 1;
			for (k = 0; k < 8; k++, row += 2, half += 2) {
				mep_cpseta1_h(round, round);
				mep_cpmadla1_h(scale8[idctScaleHalves[k][0]], *row);
				mep_cpmadua1_h(scale8[idctScaleHalves[k][1]], *half);
				mep_cpsraia1(6);
				*row = mep_cppackla1_h();
				*half = mep_cppackua1_h();
			}
		}
		if (decoder->downscale == 2) {
			row = (cp_v4hi *)table;
			for (k = 0; k < 4; k += 2) {
				mep_cpseta1_h(round, round);
				mep_cpmadla1_h(scale4[0], row[k * 2]);
				mep_cpmadua1_h(scale4[1], row[k * 2 + 2]);
				mep_cpsraia1(6);
				row[k * 2] = mep_cppackla1_h();
				row[k * 2 + 2] = mep_cppackua1_h();
			}
		}
		length -= 65 + wide;
	}
	if (length < 0)
		return SCE_JPEG_ERROR_BAD_DQT_MARKER;
	return 0;
}

/* SOF0-2: the frame, of 1 to 3 components of 8 bits */
static int32_t parseSof(JpegDecoder *decoder, int32_t length)
{
	const uint8_t *p = decoder->streamPosition;
	uint32_t colorSpace, count, sampling, h, v, blocks, table, k;
	JpegComponent *component;

	if (*p++ != 8)
		return SCE_JPEG_ERROR_BAD_PRECISION;
	if (decoder->colorSpace & JPEG_CS_SAMPLING_MASK)
		return SCE_JPEG_ERROR_SOF_DUPLICATE;
	decoder->height = p[0] << 8 | p[1];
	decoder->width = p[2] << 8 | p[3];
	p += 4;
	if (decoder->height == 0 || decoder->width == 0)
		return SCE_JPEG_ERROR_IMAGE_EMPTY;
	count = *p++;
	if (count - 1 >= 3)
		return SCE_JPEG_ERROR_COMPONENT_COUNT;
	if (count * 3 + 6 != (uint32_t)length)
		return SCE_JPEG_ERROR_BAD_SOF_MARKER;

	colorSpace = decoder->colorSpace;
	for (k = 0; k < count; k++) {
		component = &decoder->component[k];
		component->id = p[0];
		sampling = p[1];
		p += 2;
		v = sampling & 0xf;
		if (k) {
			/* the chroma: one block an MCU */
			if (sampling != 0x11)
				return SCE_JPEG_ERROR_UNSUPPORT_SAMPLING;
			blocks = v;
		} else {
			h = (sampling >> 4) & 0xf;
			if (h == 0 || v == 0 || h == 3 || v == 3 || h >= 5 || v >= 5)
				return SCE_JPEG_ERROR_UNSUPPORT_SAMPLING;
			decoder->mcuColumns = (decoder->width + h * 8 - 1) >> ((h >> 1) + 3);
			decoder->mcuRows = (decoder->height + v * 8 - 1) >> ((v >> 1) + 3);
			blocks = h * v;
			colorSpace += ((h >> 1) << 2) + (v >> 1) + 5;
		}
		table = *p++;
		if (table >= JPEG_COMPONENTS)
			return SCE_JPEG_ERROR_NO_QUANT_TABLE;
		component->blocks = blocks;
		component->quantTable = decoder->quantTable[table];
	}
	/* the components not in the frame take the last one's id */
	for (k = count; k < JPEG_COMPONENTS; k++)
		decoder->component[k].id = decoder->component[k - 1].id;

	decoder->coefsTotal = count << 6;
	switch (count) {
	case 1:
		if ((colorSpace & JPEG_CS_SAMPLING_MASK) != JPEG_CS_H1V1)
			return SCE_JPEG_ERROR_UNSUPPORT_SAMPLING;
		colorSpace = JPEG_CS_GRAYSCALE | JPEG_CS_H1V1;
		break;
	case 3:
		if (colorSpace & JPEG_CS_KIND_MASK)
			break;
		if ((decoder->component[0].id << 16 | decoder->component[1].id << 8 |
			decoder->component[2].id) == ('R' << 16 | 'G' << 8 | 'B'))
			colorSpace |= JPEG_CS_RGB;
		else
			colorSpace |= JPEG_CS_YCBCR;
		break;
	default:
		colorSpace &= JPEG_CS_SAMPLING_MASK;
		break;
	}
	decoder->colorSpace = colorSpace;
	return 0;
}

/* DRI: the restart interval */
static int32_t parseDri(JpegDecoder *decoder, int32_t length)
{
	const uint8_t *p = decoder->streamPosition;

	if (length != 2)
		return SCE_JPEG_ERROR_BAD_DRI_MARKER;
	decoder->restartInterval = p[0] << 8 | p[1];
	return 0;
}

/* SOS: the scan's components and tables, its blocks, the bits from its data */
static int32_t parseSos(JpegDecoder *decoder)
{
	const uint8_t *p = decoder->streamPosition + 2;
	int32_t headerLength = (p[-2] << 8 | p[-1]) - 3;
	uint32_t count = *p++, i, j, k, tables, blocks = 0;
	JpegComponent *component;

	if (p + headerLength >= decoder->streamEnd || count * 2 + 3 != (uint32_t)headerLength ||
		count - 1 >= 3)
		return SCE_JPEG_ERROR_BAD_SOS_MARKER;

	if (!decoder->progressive) {
		decoder->spectralEnd = 63;
		decoder->spectralStart = 0;
	} else {
		const uint8_t *end = p + headerLength;

		decoder->spectralStart = end[-3];
		decoder->spectralEnd = end[-2];
		decoder->approximationHigh = (end[-1] >> 4) & 0xf;
		decoder->approximationLow = end[-1] & 0xf;
		if ((decoder->scanPhase == JPEG_SCAN_NONE && decoder->spectralStart) ||
			(!decoder->spectralStart && decoder->spectralEnd) ||
			(decoder->spectralStart && (decoder->spectralEnd < decoder->spectralStart ||
			decoder->spectralEnd >= 64 || count != 1)) ||
			(decoder->approximationHigh &&
			decoder->approximationLow + 1 != decoder->approximationHigh) ||
			decoder->approximationLow >= 14)
			return SCE_JPEG_ERROR_BAD_PROGRESSIVE_PARAM;
		decoder->scanCoefs = decoder->approximationLow ? 0 :
			(decoder->spectralEnd - decoder->spectralStart + 1) * count;
		if (decoder->approximationHigh)
			decoder->scanRow = decoder->spectralStart ? jpegDecodeAcRefine :
				jpegDecodeDcRefine;
		else
			decoder->scanRow = decoder->spectralStart ? jpegDecodeAcFirst :
				jpegDecodeDcFirst;
	}

	for (i = 0; i < count; i++) {
		for (k = 0; k < JPEG_COMPONENTS && decoder->component[k].id != p[0]; k++)
			;
		p++;
		if (k == JPEG_COMPONENTS)
			return SCE_JPEG_ERROR_BAD_COMPONENT_ID;
		component = &decoder->component[k];
		tables = *p++;
		component->dcTable = &decoder->huffmanTable[tables >> 4][JPEG_HUFFMAN_DC];
		component->acTable = &decoder->huffmanTable[tables & 0xf][JPEG_HUFFMAN_AC];
		if ((decoder->spectralStart == 0 && !component->dcTable->defined) ||
			(decoder->spectralEnd >= 1 && !component->acTable->defined))
			return SCE_JPEG_ERROR_NO_HUFF_TABLE;
		if (count == 1) {
			decoder->mcuBlock[blocks++] = component;
		} else {
			for (j = 0; j < component->blocks; j++)
				decoder->mcuBlock[blocks++] = component;
		}
	}
	if (blocks >= JPEG_MCU_BLOCKS + 1)
		return SCE_JPEG_ERROR_BAD_MCU_SIZE;
	decoder->mcuBlock[blocks] = NULL;

	resetBits(decoder, p + 3);
	if (decoder->inputState && (decoder->bitCount >= 1 ||
		decoder->streamPosition >= decoder->streamEnd)) {
		/* no data yet: from the marker again */
		decoder->streamPosition = p - headerLength;
		return SCE_JPEG_ERROR_BAD_MARKER_LENGTH;
	}
	if (decoder->restartInterval) {
		decoder->restartMarker = JPEG_MARKER_RST0;
		decoder->restartsLeft = decoder->restartInterval;
	}
	return 0;
}

/*
 * The segments up to the next SOS or EOI read, returning that marker: the
 * position is left after the SOS marker, before its length.
 */
static int32_t nextMarker(JpegDecoder *decoder)
{
	const uint8_t *p, *saved, *segmentEnd;
	int32_t result, length;
	uint8_t marker;

	if (decoder->flags & JPEG_DECODER_DEFAULT_HUFFMAN) {
		saved = decoder->streamPosition;
		decoder->streamPosition = defaultHuffmanTables;
		result = parseDht(decoder, sizeof(defaultHuffmanTables));
		decoder->streamPosition = saved;
		if (result < 0)
			return result;
	}

	for (;;) {
		/* 0xff and a marker, past any fill bytes */
		for (p = decoder->streamPosition; ; p++) {
			if (p + 1 >= decoder->streamEnd) {
				decoder->streamPosition = decoder->streamEnd + 1;
				return SCE_JPEG_ERROR_BAD_MARKER_LENGTH;
			}
			if (p[0] == 0xff && p[1] != 0xff)
				break;
		}
		marker = p[1];
		p += 2;
		decoder->streamPosition = p;
		if (marker == JPEG_MARKER_EOI)
			return JPEG_MARKER_EOI;
		if (p + 1 >= decoder->streamEnd)
			return SCE_JPEG_ERROR_BAD_MARKER_LENGTH;
		if ((marker == JPEG_MARKER_APP1 || marker == JPEG_MARKER_APP0) &&
			!(decoder->colorSpace & JPEG_CS_KIND_MASK))
			decoder->colorSpace = JPEG_CS_YCBCR;
		length = (p[0] << 8 | p[1]) - 2;
		p += 2;
		if (length < 0)
			return SCE_JPEG_ERROR_BAD_MARKER_LENGTH;
		segmentEnd = p + length;
		if (segmentEnd >= decoder->streamEnd)
			return SCE_JPEG_ERROR_BAD_MARKER_LENGTH;
		if (marker == JPEG_MARKER_SOS)
			return JPEG_MARKER_SOS;

		decoder->streamPosition = p;
		switch (marker) {
		case 0xc2:		/* SOF2, progressive */
			decoder->progressive = 1;
			/* fall through */
		case 0xc0:		/* SOF0, baseline */
		case 0xc1:		/* SOF1, extended */
			result = parseSof(decoder, length);
			break;
		case 0xc4:		/* DHT */
			result = parseDht(decoder, length);
			break;
		case 0xdb:		/* DQT */
			result = parseDqt(decoder, length);
			break;
		case 0xdd:		/* DRI */
			result = parseDri(decoder, length);
			break;
		case 0xc3:		/* SOF3, lossless */
			return SCE_JPEG_ERROR_NO_LOSSLESS_SUPPORT;
		case 0xc9:		/* SOF9, arithmetic */
		case 0xcc:		/* DAC */
			return SCE_JPEG_ERROR_NO_ARITH_SUPPORT;
		case 0x01:		/* TEM */
		case 0xc5: case 0xc6: case 0xc7: case 0xc8: case 0xca: case 0xcb:
		case 0xcd: case 0xce: case 0xcf:
		case 0xd0: case 0xd1: case 0xd2: case 0xd3:
		case 0xd4: case 0xd5: case 0xd6: case 0xd7:
			return SCE_JPEG_ERROR_UNKNOWN_MARKER;
		default:		/* APPn, COM, DNL, ...: skipped */
			result = 0;
			break;
		}
		decoder->streamPosition = segmentEnd;
		if (result < 0)
			return result;
	}
}
