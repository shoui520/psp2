/*
 * The scan decoders (VLIW, 0x889f08-0x890ce8): one row of MCUs at a time,
 * from decoder->mcuColumn, with the bit buffer kept rotated so that the
 * next bit is always its top bit (the bits used wrap to the bottom).
 * bitCount counts the bits used less 24; past 0 three more bytes are
 * fetched behind the bits left. With input given in parts (inputState),
 * the state is saved before each MCU and an MCU that runs within 4 bytes
 * of the input's end is given back as suspended.
 */
#include "jpeg.h"

/* the bits of a scan, as the decoders keep them */
typedef struct JpegBits {
	const uint8_t *position;
	uint32_t buffer;
	int32_t count;
} JpegBits;

static uint32_t rotateLeft(uint32_t value, int32_t shift)
{
	shift &= 31;
	return shift ? value << shift | value >> (32 - shift) : value;
}

static void loadBits(JpegDecoder *decoder, JpegBits *bits)
{
	bits->position = decoder->streamPosition;
	bits->buffer = decoder->bitBuffer;
	bits->count = decoder->bitCount;
}

static void storeBits(JpegDecoder *decoder, const JpegBits *bits)
{
	decoder->bitBuffer = bits->buffer;
	decoder->bitCount = bits->count;
}

/* three bytes behind the bits left, once `used` bits past a byte are used */
static void refill(JpegDecoder *decoder, JpegBits *bits, int32_t used)
{
	if (bits->count < used)
		return;
	bits->count -= 24;
	bits->buffer = rotateLeft(jpegStreamFetch(&bits->position, decoder->streamEnd,
		rotateLeft(bits->buffer, -bits->count)), bits->count);
}

static uint32_t readBit(JpegDecoder *decoder, JpegBits *bits)
{
	uint32_t bit = bits->buffer >> 31;

	bits->buffer = rotateLeft(bits->buffer, 1);
	bits->count++;
	refill(decoder, bits, 1);
	return bit;
}

static void consume(JpegBits *bits, int32_t length)
{
	bits->buffer = rotateLeft(bits->buffer, length);
	bits->count += length;
}

/*
 * `size` bits (up to 16). Up to 8 of them may lie past the bits left; the
 * refill brings them in behind, at the bottom of the rotated buffer, and
 * the value's low byte is taken from there.
 */
static uint32_t getBits(JpegDecoder *decoder, JpegBits *bits, int32_t size)
{
	uint32_t value = bits->buffer >> (32 - size);

	consume(bits, size);
	if (bits->count < 1)
		return value;
	refill(decoder, bits, 1);
	if (bits->count >= -15)
		value = (value & 0xff00) | (bits->buffer & 0xff);
	return value;
}

/* `size` bits of a value, the negative ones below 2^(size - 1) */
static int32_t receive(JpegDecoder *decoder, JpegBits *bits, int32_t size)
{
	uint32_t negative = (bits->buffer >> 31) - 1;

	return getBits(decoder, bits, size) - (negative >> (32 - size));
}

/*
 * A code over 8 bits, from its first byte: its length l is the first whose
 * limit (the first code past it) lies above the next 16 bits. The symbol
 * (for a DC table minus the value's size) is looked up from the end.
 */
static int32_t decodeLongRest(JpegBits *bits, const JpegHuffmanTable *table, uint32_t first)
{
	uint32_t window;
	int32_t k;

	window = first << 8 | bits->buffer >> 24;
	for (k = 7; k > 0 && window >= table->limit[k]; k--)
		;
	consume(bits, 8 - k);		/* k is 16 - l */
	return (int8_t)table->longValue[247 + table->valueOffset[k] - (window >> k)];
}

static int32_t decodeLong(JpegDecoder *decoder, JpegBits *bits,
	const JpegHuffmanTable *table, uint32_t first)
{
	consume(bits, 8);
	refill(decoder, bits, 0);
	return decodeLongRest(bits, table, first);
}

/*
 * A DC difference. The lookup by the next byte gives the code's length and
 * the difference itself when both fit (length above 0), or minus the code's
 * length and minus the value's size, or 0 for a longer code.
 */
static int32_t decodeDifference(JpegDecoder *decoder, JpegBits *bits,
	const JpegHuffmanTable *table)
{
	uint32_t first = bits->buffer >> 24;
	int32_t length = table->lookupLength[first], value = table->lookupValue[first];

	if (length >= 1) {
		consume(bits, length);
		refill(decoder, bits, 0);
		return value;
	}
	if (length == 0)
		value = decodeLong(decoder, bits, table, first);
	else
		consume(bits, -length);
	refill(decoder, bits, 0);
	return value ? receive(decoder, bits, -value) : 0;
}

/* steps are in bytes */
static int16_t *advance(int16_t *coef, int32_t step)
{
	return (int16_t *)((uint8_t *)coef + step);
}

/*
 * An AC symbol: 1 with the zeros to skip and the coefficient's value, or 0
 * for a symbol without a value (ZRL with run 15, or EOBn with run n). The
 * lookup gives run - 16 * bits for a run, code and value within 8 bits,
 * the code's length with the symbol, or 40 for a longer code.
 */
static int32_t decodeAc(JpegDecoder *decoder, JpegBits *bits, const JpegHuffmanTable *table,
	int32_t *run, int32_t *value)
{
	uint32_t first = bits->buffer >> 24;
	int32_t length = table->lookupLength[first], symbol, size;

	if (length < 0) {
		consume(bits, -(length >> 4));
		*run = length & 0xf;
		*value = table->lookupValue[first];
		refill(decoder, bits, 1);
		return 1;
	}
	if (length >= 16) {
		consume(bits, 8);
		refill(decoder, bits, 0);
		symbol = decodeLongRest(bits, table, first);
	} else {
		consume(bits, length);
		symbol = table->lookupValue[first];
	}
	refill(decoder, bits, 0);
	size = symbol & 0xf;
	*run = (symbol >> 4) & 0xf;
	if (size == 0)
		return 0;
	*value = receive(decoder, bits, size);
	return 1;
}

/* the input runs out within an MCU */
static int32_t inputShort(JpegDecoder *decoder, const JpegBits *bits)
{
	return decoder->inputState && bits->position + 4 >= decoder->streamEnd;
}

/* the restart interval counted down after an MCU */
static int32_t restart(JpegDecoder *decoder, JpegBits *bits, int32_t column)
{
	int32_t result;

	if (decoder->restartsLeft == 0 || --decoder->restartsLeft)
		return 0;
	result = jpegDecoderRestart(decoder, column);
	if (result == 0)
		loadBits(decoder, bits);
	return result;
}

/* a block's samples at its component's plane, at the component's scale */
static void outputBlock(JpegComponent *component, const int16_t *block)
{
	switch (component->scale) {
	case 1:
		jpegIdct8x8(block, component->quantTable, component->plane, component->pitch);
		break;
	case 2:
		jpegIdct4x4(block, component->quantTable, component->plane, component->pitch);
		break;
	case 4:
		jpegIdct2x2(block, component->quantTable, component->plane, component->pitch);
		break;
	default:
		jpegDcOnlyPixel(block, (const int16_t *)component->quantTable, component->plane);
		break;
	}
}

/*
 * 0x889f08: a row of a baseline scan's MCUs, decoded and put out as
 * samples. Columns count from the region's left edge; those outside it are
 * decoded without output. A code past the block's end (a corrupt run) is
 * not stored here; the original wrote it beyond the block on its stack.
 */
int32_t jpegDecodeMcuRow(JpegDecoder *decoder)
{
	JpegDecoderSave save;
	JpegComponent *component;
	JpegBits bits;
	int16_t block[64];
	uint32_t width = decoder->mcuRow < decoder->regionY ? 0 : decoder->regionWidth;
	int32_t column, result, k, z, i, run, value;

	loadBits(decoder, &bits);
	for (column = decoder->mcuColumn; column < decoder->mcuColumns - decoder->regionX;
	     column++) {
		if (decoder->inputState)
			jpegDecoderSaveState(decoder, &save);
		for (k = 0; (component = decoder->mcuBlock[k]) != NULL; k++) {
			for (i = 0; i < 64; i++)
				block[i] = 0;
			component->dcPredictor += decodeDifference(decoder, &bits,
				component->dcTable);
			block[0] = component->dcPredictor;
			for (z = 0; z < 63; ) {
				z++;
				if (decodeAc(decoder, &bits, component->acTable, &run, &value)) {
					z += run;
					if (z <= 63)
						block[jpegZigzagOffset[z - 1] / 2] = value;
				} else if (run == 15) {
					z += 15;
				} else {
					z = 63;
				}
			}
			if (inputShort(decoder, &bits)) {
				result = SCE_JPEG_ERROR_INPUT_SUSPENDED;
				goto failed;
			}
			if (z > 63) {
				result = SCE_JPEG_ERROR_DECODE_ERROR;
				goto failed;
			}
			storeBits(decoder, &bits);
			decoder->streamPosition = bits.position;
			if ((uint32_t)column < width) {
				outputBlock(component, block);
				component->plane += decoder->blockStep[k];
			}
		}
		if (decoder->restartsLeft && --decoder->restartsLeft == 0) {
			result = jpegDecoderRestart(decoder, 0);
			if (result)
				goto failed;
			loadBits(decoder, &bits);
		}
	}
	decoder->mcuColumn = -decoder->regionX;
	storeBits(decoder, &bits);
	return 0;
failed:
	decoder->mcuColumn = -decoder->regionX;
	return jpegDecoderMcuResult(decoder, result, column, &save);
}

/* 0x88c448: a DC first scan; the blocks' AC coefficients are cleared */
int32_t jpegDecodeDcFirst(JpegDecoder *decoder)
{
	JpegDecoderSave save;
	JpegComponent *component;
	JpegBits bits;
	int32_t column, result, k, i;

	loadBits(decoder, &bits);
	for (column = decoder->mcuColumn; column < decoder->scanColumns; column++) {
		if (decoder->inputState)
			jpegDecoderSaveState(decoder, &save);
		for (k = 0; (component = decoder->mcuBlock[k]) != NULL; k++) {
			component->dcPredictor += decodeDifference(decoder, &bits,
				component->dcTable);
			if (inputShort(decoder, &bits)) {
				result = SCE_JPEG_ERROR_INPUT_SUSPENDED;
				goto failed;
			}
			decoder->streamPosition = bits.position;
			for (i = 0; i < 64; i++)
				component->coef[i] = 0;
			component->coef[0] = component->dcPredictor << decoder->approximationLow;
			component->coef = advance(component->coef, decoder->coefStep[k]);
		}
		storeBits(decoder, &bits);
		result = restart(decoder, &bits, column);
		if (result)
			goto failed;
	}
	decoder->mcuColumn = 0;
	storeBits(decoder, &bits);
	return 0;
failed:
	decoder->mcuColumn = 0;
	return jpegDecoderMcuResult(decoder, result, column, &save);
}

/*
 * 0x88da00: an AC first scan of the band spectralStart..spectralEnd, the
 * coefficients kept in zigzag order. With input in parts a block is decoded
 * into a copy, put back when it completed.
 */
int32_t jpegDecodeAcFirst(JpegDecoder *decoder)
{
	JpegDecoderSave save;
	JpegComponent *component;
	JpegBits bits;
	const JpegHuffmanTable *table;
	int16_t local[64], *coef;
	int32_t column, result, k, z, i, value, run;
	uint32_t eobRun = decoder->eobRun;

	loadBits(decoder, &bits);
	for (column = decoder->mcuColumn; column < decoder->scanColumns; column++) {
		if (decoder->inputState)
			jpegDecoderSaveState(decoder, &save);
		for (k = 0; (component = decoder->mcuBlock[k]) != NULL; k++) {
			if (eobRun) {
				eobRun--;
				goto next;
			}
			table = component->acTable;
			coef = component->coef;
			if (decoder->inputState) {
				for (i = 0; i < 64; i++)
					local[i] = coef[i];
				coef = local;
			}
			for (z = decoder->spectralStart - 1; z < decoder->spectralEnd; ) {
				z++;
				if (decodeAc(decoder, &bits, table, &run, &value)) {
					z += run;
					coef[z] = value << decoder->approximationLow;
				} else if (run == 15) {
					z += 15;
				} else {
					eobRun = (1 << run) - 1;
					if (run)
						eobRun += getBits(decoder, &bits, run);
					break;
				}
			}
			if (inputShort(decoder, &bits)) {
				result = SCE_JPEG_ERROR_INPUT_SUSPENDED;
				goto failed;
			}
			if (z > decoder->spectralEnd) {
				result = SCE_JPEG_ERROR_DECODE_ERROR;
				goto failed;
			}
			if (decoder->inputState)
				for (i = 0; i < 64; i++)
					component->coef[i] = local[i];
			decoder->streamPosition = bits.position;
next:
			component->coef = advance(component->coef, decoder->coefStep[k]);
		}
		storeBits(decoder, &bits);
		decoder->eobRun = eobRun;
		result = restart(decoder, &bits, column);
		if (result)
			goto failed;
		eobRun = decoder->eobRun;
	}
	decoder->mcuColumn = 0;
	storeBits(decoder, &bits);
	return 0;
failed:
	decoder->mcuColumn = 0;
	return jpegDecoderMcuResult(decoder, result, column, &save);
}

/* a refinement bit for a coefficient already non-zero, away from zero */
static void refine(JpegDecoder *decoder, JpegBits *bits, int16_t *coef, int32_t p1)
{
	if (readBit(decoder, bits))
		*coef += *coef >= 0 ? p1 : -p1;
}

/*
 * 0x88efa8: an AC refinement scan (as IJG's decode_mcu_AC_refine, without
 * its check that the bit is not set yet). The state is kept after every
 * block, so an MCU resumes at its block (scanStatus).
 */
int32_t jpegDecodeAcRefine(JpegDecoder *decoder)
{
	JpegDecoderSave save;
	JpegComponent *component;
	JpegBits bits;
	const JpegHuffmanTable *table;
	int16_t local[64], *coef;
	int32_t column, result, k, z, i, value, run;
	int32_t p1 = 1 << decoder->approximationLow;
	uint32_t eobRun = decoder->eobRun;

	loadBits(decoder, &bits);
	for (column = decoder->mcuColumn; column < decoder->scanColumns; column++) {
		for (k = decoder->scanStatus; (component = decoder->mcuBlock[k]) != NULL; k++) {
			table = component->acTable;
			coef = component->coef;
			if (decoder->inputState) {
				jpegDecoderSaveState(decoder, &save);
				for (i = 0; i < 64; i++)
					local[i] = coef[i];
				coef = local;
			}
			z = decoder->spectralStart;
			if (!eobRun) {
				for (; z <= decoder->spectralEnd; z++) {
					if (!decodeAc(decoder, &bits, table, &run, &value)) {
						if (run != 15) {
							eobRun = 1 << run;
							if (run)
								eobRun += getBits(decoder, &bits, run);
							break;
						}
						value = 0;
					}
					/* the zeros skipped refine the coefficients between */
					do {
						if (coef[z])
							refine(decoder, &bits, &coef[z], p1);
						else if (--run < 0)
							break;
						z++;
					} while (z <= decoder->spectralEnd);
					if (value)
						coef[z] = value << decoder->approximationLow;
				}
			}
			if (eobRun) {
				eobRun--;
				for (; z <= decoder->spectralEnd; z++)
					if (coef[z])
						refine(decoder, &bits, &coef[z], p1);
			}
			if (inputShort(decoder, &bits)) {
				result = SCE_JPEG_ERROR_INPUT_SUSPENDED;
				goto failed;
			}
			if (z > decoder->spectralEnd + 1) {
				result = SCE_JPEG_ERROR_DECODE_ERROR;
				goto failed;
			}
			storeBits(decoder, &bits);
			decoder->streamPosition = bits.position;
			decoder->eobRun = eobRun;
			if (decoder->inputState)
				for (i = 0; i < 64; i++)
					component->coef[i] = local[i];
			component->coef = advance(component->coef, decoder->coefStep[k]);
			decoder->scanStatus = k + 1;
		}
		if (decoder->restartsLeft && --decoder->restartsLeft == 0) {
			if (decoder->inputState)
				jpegDecoderSaveState(decoder, &save);
			result = jpegDecoderRestart(decoder, column);
			if (result)
				goto failed;
			loadBits(decoder, &bits);
			eobRun = decoder->eobRun;
		}
		decoder->scanStatus = 0;
	}
	decoder->mcuColumn = 0;
	storeBits(decoder, &bits);
	return 0;
failed:
	decoder->mcuColumn = 0;
	return jpegDecoderMcuResult(decoder, result, column, &save);
}

/* 0x88d458: a DC refinement scan's bit for each block */
int32_t jpegDecodeDcRefine(JpegDecoder *decoder)
{
	JpegDecoderSave save;
	JpegComponent *component;
	JpegBits bits;
	int32_t column, result, k;
	uint32_t bit;

	loadBits(decoder, &bits);
	for (column = decoder->mcuColumn; column < decoder->scanColumns; column++) {
		if (decoder->inputState)
			jpegDecoderSaveState(decoder, &save);
		for (k = 0; (component = decoder->mcuBlock[k]) != NULL; k++) {
			bit = readBit(decoder, &bits);
			if (inputShort(decoder, &bits)) {
				result = SCE_JPEG_ERROR_INPUT_SUSPENDED;
				goto failed;
			}
			decoder->streamPosition = bits.position;
			*component->coef |= bit << decoder->approximationLow;
			component->coef = advance(component->coef, decoder->coefStep[k]);
		}
		storeBits(decoder, &bits);
		result = restart(decoder, &bits, column);
		if (result)
			goto failed;
	}
	decoder->mcuColumn = 0;
	storeBits(decoder, &bits);
	return 0;
failed:
	decoder->mcuColumn = 0;
	return jpegDecoderMcuResult(decoder, result, column, &save);
}

/*
 * 0x890ce8: a row of a progressive image's blocks into samples, for the
 * component of the scan. Columns count from the region's left edge.
 */
int32_t jpegOutputComponentRow(JpegDecoder *decoder)
{
	JpegComponent *component;
	int16_t block[64];
	uint32_t width = decoder->mcuRow < decoder->regionY ? 0 : decoder->regionWidth;
	int32_t column, i;

	for (column = decoder->mcuColumn; column < decoder->blockColumns - decoder->regionX;
	     column++) {
		component = decoder->mcuBlock[0];
		if ((uint32_t)column >= width) {
			component->coef = advance(component->coef, decoder->coefStep[0]);
			continue;
		}
		block[0] = component->coef[0];
		for (i = 1; i < 64; i++)
			block[jpegZigzagOffset[i - 1] / 2] = component->coef[i];
		component->coef = advance(component->coef, decoder->coefStep[0]);
		outputBlock(component, block);
		component->plane += decoder->blockStep[0];
	}
	decoder->mcuColumn = -decoder->regionX;
	return 0;
}
