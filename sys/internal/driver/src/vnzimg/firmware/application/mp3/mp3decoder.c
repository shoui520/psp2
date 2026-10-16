/*
 * The MP3 decoder's frame (0x869910-0x86b868, 0x874ae4-0x874ca4): the
 * stream's sync and header, the side information, the scale factors, the
 * bit reservoir's fill, the joint stereo bands and the de-emphasis; the
 * granules' signal processing is the VLIW kernels'.
 */
#include "mp3.h"

#define SYNC_MASK		0xffe00000
#define HEADER_KEY_MASK		0xfffe0c00	/* sync, version, layer, sampling rate */
#define HEADER_MPEG1		0x100000
#define HEADER_LAYER		0x60000
#define RESERVOIR_MASK		(MP3_RESERVOIR_WORDS - 1)
#define RESERVOIR_WRAP		2048		/* bytes the fill and position are kept under */
#define POLYPHASE_OFFSET	288
#define INTENSITY_MAX_MPEG1	7
#define SHORT_BOUNDS_OFFSET	(MP3_LONG_BANDS + 1)

/* RESERVOIR_WRAP must be the ring's size */
typedef char mp3ReservoirWrap[RESERVOIR_WRAP == MP3_RESERVOIR_WORDS * 4 ? 1 : -1];

void mp3Copy(void *dst, const void *src, int32_t size)
{
	uint8_t *to = dst;
	const uint8_t *from = src;

	while (size-- > 0)
		*to++ = *from++;
}

static void fill(void *dst, uint8_t value, int32_t size)
{
	uint8_t *to = dst;

	while (size-- > 0)
		*to++ = value;
}

/* the first byte of a word in memory is its most significant */
static int hostBigEndian(void)
{
	volatile uint32_t word = 0x41424344;

	return *(volatile uint8_t *)&word == 'A';
}

static void decoderClear(Mp3Decoder *decoder)
{
	fill(decoder, 0, sizeof(*decoder));
	decoder->header.initialized = 1;
	decoder->header.layer = 3;
	decoder->sideReader.word = NULL;
	decoder->sideReader.bit = 0;
	decoder->input = NULL;
	decoder->output = NULL;
	decoder->outputSize = NULL;
	decoder->needSync = 1;
	decoder->started = 1;
	decoder->interleaved = 1;
	decoder->bytesPerSample = 2;
	decoder->warning = 0;
}

/* 0x84d6f8 */
int32_t mp3DecoderInit(Mp3Decoder *decoder)
{
	if (!decoder)
		return 1;
	decoderClear(decoder);
	decoder->bigEndianHost = hostBigEndian();
	decoder->bigEndianOutput = decoder->bigEndianHost;
	decoder->synthesis = &decoder->synthesisState;
	decoder->reserved11 = 0;
	decoder->reserved4 = 0;
	decoder->synthesisState.polyphaseOffset = POLYPHASE_OFFSET;
	decoder->emphasis = -1;
	return 0;
}

/* 0x84d770: as made, but the synthesis is left cleared */
int32_t mp3DecoderReset(Mp3Decoder *decoder)
{
	uint8_t bigEndian;

	if (!decoder)
		return 1;
	decoderClear(decoder);
	bigEndian = hostBigEndian();
	decoder->reserved11 = 0;
	decoder->reserved4 = 0;
	decoder->bigEndianOutput = bigEndian;
	decoder->bigEndianHost = bigEndian;
	return 0;
}

/* count bits (0-32) of the side information */
static uint32_t sideRead(Mp3BitReader *reader, uint32_t count)
{
	uint32_t value;
	int32_t next;

	if (!count)
		return 0;
	value = *reader->word << reader->bit >> (32 - count);
	next = reader->bit + count - 32;
	if (next < 0) {
		reader->bit += count;
		return value;
	}
	reader->word++;
	if (next >= 1)
		value |= *reader->word >> (32 - next);
	reader->bit = next;
	return value;
}

/* 0x869c08: a header word; a status of 22 or more is a warning */
static int32_t headerParse(Mp3Header *header, uint32_t word)
{
	int32_t result;

	header->id = (word >> 19) & 1;
	header->samplingIndex = (word >> 10) & 3;
	if (header->samplingIndex == 3)
		return MP3_STATUS_SAMPLING_RATE;
	header->samplingRate = mp3SamplingRates[header->samplingIndex];
	if (!header->id) {
		/* MPEG-2, and MPEG-2.5 with the sync's last bit clear */
		header->samplingRate >>= 1;
		header->samplingIndex += 3;
		if (!(word & 0x180000)) {
			header->samplingRate >>= 1;
			header->samplingIndex += 3;
		}
	} else if (!(word & HEADER_MPEG1)) {
		return MP3_STATUS_ID;
	}

	header->layer = 4 - ((word >> 17) & 3);
	if (header->layer == 4)
		return MP3_STATUS_LAYER;
	header->protection = (word >> 16) & 1;
	header->bitrateIndex = (word >> 12) & 15;
	if (header->bitrateIndex == 15)
		return MP3_STATUS_FREE_FORMAT;
	header->bitrate = mp3Bitrates[header->id ? 0 : 1][header->bitrateIndex];

	result = 0;
	header->padding = (word >> 9) & 1;
	if (header->padding && header->samplingRate != 44100 && header->samplingRate != 22050 &&
	    header->samplingRate != 11025 && header->bitrateIndex != 0)
		result = MP3_STATUS_PADDING;
	header->privateBit = (word >> 8) & 1;
	header->mode = (word >> 6) & 3;
	header->modeExtension = (word >> 4) & 3;
	if (header->mode == MP3_MODE_MONO && header->modeExtension)
		return MP3_STATUS_STEREO;
	header->copyright = (word >> 3) & 1;
	header->emphasis = word & 3;
	header->original = (word >> 2) & 1;
	if (header->emphasis == 2 || header->emphasis == 3)
		return result == MP3_STATUS_PADDING ? MP3_STATUS_BAD_HEADER : MP3_STATUS_WARNING;
	return result;
}

/* 0x869d7a: the frame's bytes, and its main data's */
static uint32_t frameSize(Mp3Header *header)
{
	uint32_t factor = 144, size, side;

	if (header->layer == 3 && header->id == 0)
		factor = 72;
	size = factor * header->bitrate / header->samplingRate + header->padding;
	if (header->layer == 3) {
		if (header->id == 1)
			side = header->mode == MP3_MODE_MONO ? 17 : 32;
		else
			side = header->mode == MP3_MODE_MONO ? 9 : 17;
		header->mainDataSize = size - side - (header->protection ? 0 : 2) - 4;
	}
	header->frameSize = size;
	return size;
}

/* the status of a header that differs from the stream's */
static int32_t headerChange(Mp3Decoder *decoder, uint32_t word)
{
	if (!(word & HEADER_MPEG1))
		return MP3_STATUS_ID;
	if (!(word & HEADER_LAYER))
		return MP3_STATUS_LAYER;
	if (((word >> 10) & 3) == 3)
		return MP3_STATUS_SAMPLING_RATE;
	if (((word >> 19) & 1) != ((decoder->headerKey >> 19) & 1))
		return MP3_STATUS_VERSION_CHANGE;
	if ((decoder->headerKey & HEADER_LAYER) != (word & HEADER_LAYER))
		return MP3_STATUS_LAYER_CHANGE;
	return MP3_STATUS_VERSION_CHANGE;
}

/*
 * 0x8699b6: the next frame in the input, copied out with its words in big
 * endian order; the side information reader is set at its start.
 */
static int32_t frameSync(Mp3Decoder *decoder, int32_t *available, int32_t *left)
{
	const uint8_t *in = decoder->input + (decoder->bitPosition >> 3);
	int32_t count = (decoder->bitEnd - decoder->bitPosition + 1) / 8;
	int32_t result, size, found = 0;
	uint32_t word, *words, *last;
	uint8_t *frame;

	*available = count;
	decoder->bitsUsed = 0;
	decoder->lastFrameSize = 0;
	if (count < 4)
		return MP3_STATUS_SHORT_INPUT;
	word = in[0] << 16 | in[1] << 8 | in[2];
	in += 3;
	for (;;) {
		word = word << 8 | *in++;
		if (decoder->needSync ? (word & SYNC_MASK) == SYNC_MASK :
		    (word & HEADER_KEY_MASK) == decoder->headerKey) {
			result = headerParse(&decoder->header, word);
			if (result == MP3_STATUS_BAD_HEADER)
				goto bad;
			if (result >= MP3_STATUS_WARNING)
				decoder->warning = result;
			else if (result)
				goto bad;
			if (decoder->needSync)
				decoder->headerKey = word & HEADER_KEY_MASK;
			found = 1;
		} else if (!decoder->needSync && (word & SYNC_MASK) == SYNC_MASK) {
			decoder->bitsUsed = (*available - count + 1) * 8;
			return headerChange(decoder, word);
		}
		if (found)
			break;
		count--;
		if (count < 4) {
			decoder->bitsUsed = (*available - count) * 8;
			return MP3_STATUS_NO_SYNC;
		}
	}

	*left = count;
	count -= 4;
	if (!decoder->header.bitrate)
		return MP3_STATUS_FREE_FORMAT;
	size = frameSize(&decoder->header) - 4;
	frame = decoder->frame + (decoder->header.protection ? 0 : 2);
	if (count < size) {
		decoder->bitsUsed = (*available - count) * 8 - 32;
		return MP3_STATUS_SHORT_INPUT;
	}
	mp3Copy(frame, in, size);
	count -= size;
	decoder->bitsUsed = (*available - count) * 8;
	if (!decoder->header.protection) {
		mp3CrcInit(&decoder->crc);
		mp3CrcUpdate(&decoder->crc, word, 16);
		size -= 2;
		decoder->crcRead = frame[0] << 8 | frame[1];
		frame += 2;
	}
	decoder->sideReader.word = (uint32_t *)frame;
	decoder->sideReader.bit = 0;
	if (!decoder->bigEndianHost) {
		words = (uint32_t *)frame;
		last = words + ((size - 1) >> 2);
		for (; words <= last; words++)
			*words = *words >> 24 | (*words >> 8 & 0xff00) | (*words << 8 & 0xff0000) |
				*words << 24;
	}
	return 0;

bad:
	decoder->bitsUsed = (*available - count + 1) * 8;
	return result;
}

/* a granule's window switching fields, with their CRC bits */
static int32_t sideWindow(Mp3BitReader *reader, Mp3Granule *granule, Mp3Crc *crc,
	uint32_t crcOn)
{
	uint32_t check = 0, gains;

	granule->blockType = sideRead(reader, 2);
	granule->mixedBlock = sideRead(reader, 1);
	granule->tableSelect[0] = sideRead(reader, 5);
	granule->tableSelect[1] = sideRead(reader, 5);
	if (crcOn)
		check = granule->blockType << 11 | granule->mixedBlock << 10 |
			granule->tableSelect[0] << 5 | granule->tableSelect[1];
	granule->subblockGain[0] = sideRead(reader, 3);
	granule->subblockGain[1] = sideRead(reader, 3);
	granule->subblockGain[2] = sideRead(reader, 3);
	if (crcOn) {
		gains = granule->subblockGain[0] << 6 | granule->subblockGain[1] << 3 |
			granule->subblockGain[2];
		mp3CrcUpdate(crc, check, 13);
		mp3CrcUpdate(crc, gains, 9);
	}
	if (!granule->blockType)
		return MP3_STATUS_BLOCK_TYPE;
	if (granule->blockType == MP3_BLOCK_SHORT && !granule->mixedBlock)
		granule->region0Count = 8;
	else
		granule->region0Count = 7;
	granule->region1Count = 20 - granule->region0Count;
	return 0;
}

/* a granule's three regions, with their CRC bits */
static void sideRegions(Mp3BitReader *reader, Mp3Granule *granule, Mp3Crc *crc,
	uint32_t crcOn)
{
	uint32_t tables = 0;

	granule->tableSelect[0] = sideRead(reader, 5);
	granule->tableSelect[1] = sideRead(reader, 5);
	granule->tableSelect[2] = sideRead(reader, 5);
	if (crcOn)
		tables = granule->tableSelect[0] << 10 | granule->tableSelect[1] << 5 |
			granule->tableSelect[2];
	granule->region0Count = sideRead(reader, 4);
	granule->region1Count = sideRead(reader, 3);
	granule->blockType = 0;
	granule->mixedBlock = 0;
	if (crcOn) {
		mp3CrcUpdate(crc, tables, 15);
		mp3CrcUpdate(crc, granule->region0Count << 3 | granule->region1Count, 7);
	}
}

/* 0x86ae50 */
static int32_t sideInfo(Mp3Decoder *decoder, uint32_t channels)
{
	Mp3BitReader *reader = &decoder->sideReader;
	uint32_t crcOn = !decoder->header.protection;
	Mp3SideInfo *side = &decoder->side;
	Mp3Crc *crc = &decoder->crc;
	uint32_t ch, gr, granules, s[4];
	Mp3Granule *granule;
	int32_t result;

	decoder->sideReader.bit = 0;
	if (decoder->header.id == 1) {
		side->mainDataBegin = sideRead(reader, 9);
		if (channels == 1) {
			side->privateBits = sideRead(reader, 5);
			if (crcOn)
				mp3CrcUpdate(crc, side->mainDataBegin << 5 | side->privateBits, 14);
		} else {
			side->privateBits = sideRead(reader, 3);
			if (crcOn)
				mp3CrcUpdate(crc, side->mainDataBegin << 3 | side->privateBits, 12);
		}
		decoder->header.privateBits = side->privateBits;
		for (ch = 0; ch < channels; ch++) {
			s[0] = side->channel[ch].scfsi[0] = sideRead(reader, 1);
			s[1] = side->channel[ch].scfsi[1] = sideRead(reader, 1);
			s[2] = side->channel[ch].scfsi[2] = sideRead(reader, 1);
			s[3] = side->channel[ch].scfsi[3] = sideRead(reader, 1);
			if (crcOn)
				mp3CrcUpdate(crc, s[0] << 3 | s[1] << 2 | s[2] << 1 | s[3], 4);
		}
		for (gr = 0; gr < 2; gr++) {
			for (ch = 0; ch < channels; ch++) {
				granule = &side->channel[ch].granule[gr];
				granule->part23Length = sideRead(reader, 12);
				granule->bigValues = sideRead(reader, 9);
				granule->globalGain = sideRead(reader, 8);
				granule->scalefacCompress = sideRead(reader, 4);
				granule->windowSwitching = sideRead(reader, 1);
				if (crcOn) {
					mp3CrcUpdate(crc, granule->part23Length, 12);
					mp3CrcUpdate(crc, granule->bigValues, 9);
					mp3CrcUpdate(crc, granule->globalGain << 5 |
						granule->scalefacCompress << 1 |
						granule->windowSwitching, 13);
				}
				if (granule->windowSwitching) {
					result = sideWindow(reader, granule, crc, crcOn);
					if (result)
						return result;
				} else {
					sideRegions(reader, granule, crc, crcOn);
				}
				granule->preflag = sideRead(reader, 1);
				granule->scalefacScale = sideRead(reader, 1);
				granule->count1TableSelect = sideRead(reader, 1);
				if (crcOn)
					mp3CrcUpdate(crc, granule->preflag << 2 |
						granule->scalefacScale << 1 |
						granule->count1TableSelect, 3);
			}
		}
	} else {
		side->mainDataBegin = sideRead(reader, 8);
		if (channels == 1) {
			side->privateBits = sideRead(reader, 1);
			if (crcOn)
				mp3CrcUpdate(crc, side->mainDataBegin << 1 | side->privateBits, 9);
		} else {
			side->privateBits = sideRead(reader, 2);
			if (crcOn)
				mp3CrcUpdate(crc, side->mainDataBegin << 2 | side->privateBits, 10);
		}
		decoder->header.privateBits = side->privateBits;
		for (ch = 0; ch < channels; ch++) {
			granule = &side->channel[ch].granule[0];
			granule->part23Length = sideRead(reader, 12);
			granule->bigValues = sideRead(reader, 9);
			granule->globalGain = sideRead(reader, 8);
			granule->scalefacCompress = sideRead(reader, 9);
			granule->windowSwitching = sideRead(reader, 1);
			if (crcOn) {
				mp3CrcUpdate(crc, granule->part23Length, 12);
				mp3CrcUpdate(crc, granule->bigValues, 9);
				mp3CrcUpdate(crc, granule->globalGain << 10 |
					granule->scalefacCompress << 1 | granule->windowSwitching, 18);
			}
			if (granule->windowSwitching) {
				result = sideWindow(reader, granule, crc, crcOn);
				if (result)
					return result;
			} else {
				sideRegions(reader, granule, crc, crcOn);
			}
			granule->scalefacScale = sideRead(reader, 1);
			granule->count1TableSelect = sideRead(reader, 1);
			if (crcOn)
				mp3CrcUpdate(crc, granule->scalefacScale << 1 |
					granule->count1TableSelect, 2);
		}
	}

	/* joint stereo needs the channels' blocks alike */
	if (decoder->header.mode == MP3_MODE_JOINT_STEREO && decoder->header.modeExtension) {
		granules = (0 < decoder->header.id) + 1;
		for (gr = 0; gr < granules; gr++) {
			if (side->channel[1].granule[gr].blockType !=
			    side->channel[0].granule[gr].blockType ||
			    side->channel[1].granule[gr].windowSwitching !=
			    side->channel[0].granule[gr].windowSwitching ||
			    side->channel[1].granule[gr].mixedBlock !=
			    side->channel[0].granule[gr].mixedBlock)
				return MP3_STATUS_STEREO;
		}
	}
	return 0;
}

static void readBands(Mp3Reservoir *reservoir, int32_t *band, uint32_t count, uint32_t length)
{
	uint32_t i;

	for (i = 0; i < count; i++)
		band[i] = mp3ReservoirRead(reservoir, length);
}

/* 0x86a514: MPEG-1 scale factors; returns the bits read */
static int32_t scalefactors(Mp3Decoder *decoder, uint32_t ch, uint32_t gr)
{
	const Mp3Granule *granule = &decoder->side.channel[ch].granule[gr];
	const uint32_t *scfsi = decoder->side.channel[ch].scfsi;
	Mp3Scalefactors *sf = &decoder->scalefactors[ch];
	Mp3Reservoir *reservoir = &decoder->reservoir;
	uint32_t slen1 = mp3ScalefactorLengths[0][granule->scalefacCompress];
	uint32_t slen2 = mp3ScalefactorLengths[1][granule->scalefacCompress];
	int32_t start = reservoir->bitCount;
	uint32_t band, window;

	if (granule->windowSwitching && granule->blockType == MP3_BLOCK_SHORT) {
		if (granule->mixedBlock) {
			readBands(reservoir, sf->longBand, 8, slen1);
			for (band = 3; band < 6; band++)
				readBands(reservoir, sf->shortBand[band], 3, slen1);
		} else {
			for (band = 0; band < 6; band++)
				readBands(reservoir, sf->shortBand[band], 3, slen1);
		}
		for (band = 6; band < 12; band++)
			readBands(reservoir, sf->shortBand[band], 3, slen2);
		for (window = 0; window < 3; window++)
			sf->shortBand[12][window] = 0;
	} else {
		if (!scfsi[0] || !gr)
			readBands(reservoir, &sf->longBand[0], 6, slen1);
		if (!scfsi[1] || !gr)
			readBands(reservoir, &sf->longBand[6], 5, slen1);
		if (!scfsi[2] || !gr)
			readBands(reservoir, &sf->longBand[11], 5, slen2);
		if (!scfsi[3] || !gr)
			readBands(reservoir, &sf->longBand[16], 5, slen2);
		sf->longBand[21] = 0;
		sf->longBand[22] = 0;
	}
	return reservoir->bitCount - start;
}

/*
 * 0x86aaa4: MPEG-2 scale factors, their lengths from scalefac_compress
 * (halved for the intensity coded right channel); returns the bits read.
 */
static int32_t lsfScalefactors(Mp3Decoder *decoder, uint32_t ch, uint32_t gr)
{
	Mp3Granule *granule = &decoder->side.channel[ch].granule[gr];
	Mp3Scalefactors *sf = &decoder->scalefactors[ch];
	Mp3Reservoir *reservoir = &decoder->reservoir;
	int32_t start = reservoir->bitCount, compress, value, max;
	int16_t lengths[4] = { 0, 0, 0, 0 };
	int8_t values[54];
	uint32_t table = 0, kind = 0, i, k, band, window, n = 0;

	fill(values, 0, sizeof(values));
	for (i = 0; i < MP3_LSF_BANDS; i++)
		decoder->lsfIllegal[i] = 0;
	if (granule->blockType == MP3_BLOCK_SHORT && !granule->mixedBlock)
		kind = 1;
	if (granule->blockType == MP3_BLOCK_SHORT && granule->mixedBlock == 1)
		kind = 2;

	compress = granule->scalefacCompress;
	if (!((decoder->header.modeExtension == 1 || decoder->header.modeExtension == 3) &&
	      ch == 1)) {
		if (compress < 400) {
			lengths[0] = (compress >> 4) / 5;
			lengths[1] = (compress >> 4) % 5;
			lengths[2] = (compress & 15) >> 2;
			lengths[3] = compress & 3;
			table = 0;
			granule->preflag = 0;
		} else if (compress < 500) {
			compress -= 400;
			lengths[0] = (compress >> 2) / 5;
			lengths[1] = (compress >> 2) % 5;
			lengths[2] = compress & 3;
			lengths[3] = 0;
			table = 1;
			granule->preflag = 0;
		} else if (compress < 512) {
			compress -= 500;
			lengths[0] = compress / 3;
			lengths[1] = compress % 3;
			lengths[2] = 0;
			lengths[3] = 0;
			table = 2;
			granule->preflag = 1;
		}
	}
	if ((decoder->header.modeExtension == 1 || decoder->header.modeExtension == 3) &&
	    ch == 1) {
		compress = (int16_t)(compress >> 1);
		if (compress < 180) {
			lengths[0] = compress / 36;
			lengths[1] = (compress % 36) / 6;
			lengths[2] = (compress % 36) % 6;
			lengths[3] = 0;
			table = 3;
			granule->preflag = 0;
		} else if (compress < 244) {
			compress -= 180;
			lengths[0] = (compress & 63) >> 4;
			lengths[1] = (compress & 15) >> 2;
			lengths[2] = compress & 3;
			lengths[3] = 0;
			table = 4;
			granule->preflag = 0;
		} else if (compress < 255) {
			compress -= 244;
			lengths[0] = compress / 3;
			lengths[1] = compress % 3;
			lengths[2] = 0;
			lengths[3] = 0;
			table = 5;
			granule->preflag = 0;
		}
	}

	for (i = 0; i < 4; i++) {
		max = (1 << lengths[i]) - 1;
		for (k = 0; k < mp3LsfBands[table][kind][i]; k++) {
			if (!lengths[i]) {
				values[n] = 0;
			} else {
				value = (int8_t)mp3ReservoirRead(reservoir, lengths[i]);
				values[n] = value;
				decoder->lsfIllegal[n] = value == max;
			}
			n++;
		}
	}

	n = 0;
	if (granule->windowSwitching && granule->blockType == MP3_BLOCK_SHORT) {
		if (granule->mixedBlock) {
			for (band = 0; band < 6; band++)
				sf->longBand[band] = values[n++];
			band = 3;
		} else {
			band = 0;
		}
		for (; band < 12; band++)
			for (window = 0; window < 3; window++)
				sf->shortBand[band][window] = values[n++];
		for (window = 0; window < 3; window++)
			sf->shortBand[12][window] = 0;
	} else {
		for (band = 0; band < 21; band++)
			sf->longBand[band] = values[n++];
		sf->longBand[21] = 0;
	}
	return reservoir->bitCount - start;
}

/* the bands from the right channel's last nonzero one up are intensity coded */
static uint32_t lastNonzeroBand(const uint8_t *widths, const int32_t *right, uint32_t *position,
	uint32_t limit, uint32_t *band)
{
	uint32_t last = 0, width, i;

	do {
		width = widths[(*band)++];
		for (i = 0; i < width; i++) {
			if (right[i]) {
				last = *band;
				break;
			}
		}
		*position += width;
		right += width;
	} while (*position < limit);
	return last;
}

/* 0x86b4e8: a granule's joint stereo, band by band */
static void stereo(Mp3Decoder *decoder, uint32_t gr)
{
	const Mp3Header *header = &decoder->header;
	const Mp3Granule *right1 = &decoder->side.channel[1].granule[gr];
	const Mp3BandWidths *bands = &mp3BandWidths[header->samplingIndex];
	Mp3Scalefactors *sf = &decoder->scalefactors[1];
	int32_t *left = decoder->synthesis->xr[0];
	int32_t *right = decoder->synthesis->xr[1];
	uint32_t position, band, last, lastShort, longBands, window, width, i;
	uint32_t windowLast[3] = { 0, 0, 0 };
	uint16_t flags[MP3_LSF_BANDS];
	const int32_t *positions, *ratios;
	const uint8_t *widths;
	int32_t *illegal = decoder->lsfIllegal;
	int32_t *bandRight;

	if (!(header->modeExtension & MP3_MODE_EXT_INTENSITY)) {
		if (header->modeExtension == MP3_MODE_EXT_MS)
			mp3StereoMs(left, right, MP3_GRANULE_SAMPLES);
		return;
	}
	for (i = 0; i < MP3_LSF_BANDS; i++)
		flags[i] = header->modeExtension;

	if (right1->blockType == MP3_BLOCK_SHORT) {
		widths = bands->shortWidths;
		positions = &sf->shortBand[0][0];
		last = 0;
		position = 0;
		band = 0;
		longBands = 0;
		bandRight = right;
		if (right1->mixedBlock) {
			widths = bands->mixedWidths;
			i = mp3BandBounds[header->samplingIndex].longBounds[header->samplingIndex >= 3 ? 6 : 8];
			if (i)
				last = lastNonzeroBand(widths, bandRight, &position, i, &band);
			bandRight = right + position;
			longBands = band;
		}
		lastShort = 0;
		window = 0;
		while (position < MP3_GRANULE_SAMPLES) {
			width = widths[band++];
			for (i = 0; i < width; i++) {
				if (bandRight[i]) {
					lastShort = band;
					windowLast[window] = band;
					break;
				}
			}
			window = (window + 1) % 3;
			position += width;
			bandRight += width;
		}
		if (lastShort)
			last = longBands;
		for (i = 0; i < last; i++)
			flags[i] = header->modeExtension & ~MP3_MODE_EXT_INTENSITY;
		window = 0;
		for (i = longBands; i < lastShort; i++) {
			if (i < windowLast[window])
				flags[i] = header->modeExtension & ~MP3_MODE_EXT_INTENSITY;
			window = (window + 1) % 3;
		}
		if (lastShort >= 36) {
			sf->shortBand[12][2] = 0;
			sf->shortBand[12][1] = 0;
			sf->shortBand[12][0] = 0;
			illegal[36] = 0;
			illegal[37] = 0;
			illegal[38] = 0;
		} else {
			sf->shortBand[12][2] = sf->shortBand[11][2];
			sf->shortBand[12][1] = sf->shortBand[11][1];
			sf->shortBand[12][0] = sf->shortBand[11][0];
			illegal[38] = illegal[35];
			illegal[37] = illegal[34];
			illegal[36] = illegal[33];
		}
	} else {
		widths = bands->longWidths;
		positions = sf->longBand;
		position = 0;
		band = 0;
		last = lastNonzeroBand(widths, right, &position, MP3_GRANULE_SAMPLES, &band);
		for (i = 0; i < last; i++)
			flags[i] = header->modeExtension & ~MP3_MODE_EXT_INTENSITY;
		if (last == 21) {
			sf->longBand[21] = 0;
			illegal[21] = 0;
		} else {
			sf->longBand[21] = sf->longBand[20];
			illegal[21] = illegal[20];
		}
	}

	position = 0;
	band = 0;
	if (header->id == 0) {
		ratios = mp3LsfIntensity[right1->scalefacCompress & 1];
		do {
			width = widths[band];
			if (!illegal[band] && (flags[band] & MP3_MODE_EXT_INTENSITY))
				mp3StereoIntensityLsf(left + position, right + position, positions[band],
					ratios, width);
			else if (header->modeExtension & MP3_MODE_EXT_MS)
				mp3StereoMs(left + position, right + position, width);
			band++;
			position += width;
		} while (position < MP3_GRANULE_SAMPLES);
	} else {
		do {
			width = widths[band];
			if ((uint32_t)positions[band] < INTENSITY_MAX_MPEG1 &&
			    (flags[band] & MP3_MODE_EXT_INTENSITY))
				mp3StereoIntensity(left + position, right + position, positions[band],
					width);
			else if (header->modeExtension & MP3_MODE_EXT_MS)
				mp3StereoMs(left + position, right + position, width);
			band++;
			position += width;
		} while (position < MP3_GRANULE_SAMPLES);
	}
}

/* 0x874c8c */
static void emphasisReset(Mp3Decoder *decoder, uint8_t emphasis)
{
	decoder->emphasis = emphasis;
	decoder->filterOutput[1] = 0;
	decoder->filterOutput[0] = 0;
	decoder->filterInput[1] = 0;
	decoder->filterInput[0] = 0;
}

/* 0x874ae4: 50/15 us de-emphasis of an MPEG-1 frame's 16-bit output */
static void deemphasis(uint32_t channels, uint32_t samplingRate, int16_t *pcm,
	Mp3Decoder *decoder)
{
	const int16_t *filter;
	uint32_t i, ch, count;
	int16_t input;

	if (samplingRate == 32000)
		filter = mp3EmphasisFilters[1];
	else if (samplingRate == 48000)
		filter = mp3EmphasisFilters[2];
	else if (samplingRate == 44100)
		filter = mp3EmphasisFilters[0];
	else
		return;
	if (channels != 2)
		channels = 1;
	count = MP3_GRANULE_SAMPLES * 2 / channels;
	for (i = 0; i < count; i++) {
		for (ch = 0; ch < channels; ch++) {
			input = *pcm;
			*pcm = (input * filter[0] + decoder->filterInput[ch] * filter[1] +
				decoder->filterOutput[ch] * filter[2]) >> 15;
			decoder->filterInput[ch] = input;
			decoder->filterOutput[ch] = *pcm;
			pcm++;
		}
	}
}

/* main data bytes from the frame into the reservoir, a word at a time */
static void reservoirFill(Mp3Decoder *decoder, int32_t size)
{
	Mp3Reservoir *reservoir = &decoder->reservoir;
	Mp3BitReader *reader = &decoder->sideReader;
	const uint32_t *word;
	uint32_t value, current;
	int32_t take, words;

	if (reservoir->pending) {
		take = 4 - reservoir->pending;
		if (take > size)
			take = size;
		value = sideRead(reader, take * 8);
		reservoir->pending += take;
		reservoir->words[reservoir->writeIndex] |= value << ((4 - reservoir->pending) * 8);
		if (reservoir->pending >= 4) {
			reservoir->pending &= 3;
			reservoir->writeIndex = (reservoir->writeIndex + 1) & RESERVOIR_MASK;
		}
		size -= take;
	}
	if (!size)
		return;
	words = size >> 2;
	word = reader->word;
	if (reader->bit) {
		current = *word;
		while (words-- >= 1) {
			word++;
			reservoir->words[reservoir->writeIndex] = current << reader->bit |
				*word >> (32 - reader->bit);
			current = *word;
			reservoir->writeIndex = (reservoir->writeIndex + 1) & RESERVOIR_MASK;
		}
	} else {
		while (words-- >= 1) {
			reservoir->words[reservoir->writeIndex] = *word++;
			reservoir->writeIndex = (reservoir->writeIndex + 1) & RESERVOIR_MASK;
		}
	}
	reader->word = word;
	reservoir->pending = size & 3;
	if (reservoir->pending)
		reservoir->words[reservoir->writeIndex] = sideRead(reader, reservoir->pending * 8)
			<< ((4 - reservoir->pending) * 8);
}

/* 0x869e14: a synced frame's granules into the output */
static int32_t frameDecode(Mp3Decoder *decoder)
{
	const Mp3Header *header = &decoder->header;
	Mp3Reservoir *reservoir = &decoder->reservoir;
	uint32_t channels, granules, ch, gr, stride, frameBytes, antialias;
	int32_t mainData = header->mainDataSize, position, available, part2, result;
	uint8_t *pointers[4];
	const Mp3Granule *granule;
	Mp3Synthesis *synthesis;
	uint16_t longBands;

	if (decoder->needSync == 1) {
		mp3Copy(decoder->shortWidths, mp3BandWidths[header->samplingIndex].shortWidths,
			sizeof(decoder->shortWidths));
		mp3Copy(decoder->longBounds, mp3BandBounds[header->samplingIndex].longBounds,
			sizeof(decoder->longBounds));
	}
	channels = header->mode == MP3_MODE_MONO ? 1 : 2;
	granules = (0 < header->id) + 1;
	frameBytes = decoder->bytesPerSample * MP3_GRANULE_SAMPLES * granules;

	/* where each granule's channels go */
	if (decoder->interleaved == 1) {
		if (decoder->outputSize[0] < channels * frameBytes)
			return MP3_STATUS_NO_OUTPUT;
		for (gr = 0; gr < granules; gr++) {
			for (ch = 0; ch < channels; ch++) {
				if (!decoder->output[0])
					return MP3_STATUS_NO_OUTPUT;
				pointers[gr * 2 + ch] = decoder->output[0] +
					decoder->bytesPerSample * MP3_GRANULE_SAMPLES * gr * channels +
					decoder->bytesPerSample * ch;
			}
		}
		stride = decoder->bytesPerSample * channels;
	} else {
		for (gr = 0; gr < granules; gr++) {
			for (ch = 0; ch < channels; ch++) {
				if (!decoder->output[ch])
					return MP3_STATUS_NO_OUTPUT;
				pointers[gr * 2 + ch] = decoder->output[ch] +
					decoder->bytesPerSample * MP3_GRANULE_SAMPLES * gr;
			}
		}
		stride = decoder->bytesPerSample;
	}

	result = sideInfo(decoder, channels);
	if (result)
		return result;
	if (!header->protection && decoder->crcRead != decoder->crc.value)
		return MP3_STATUS_CRC;

	/* the main data into the reservoir, and the read position to this frame's */
	reservoirFill(decoder, mainData);
	position = reservoir->bitCount >> 3;
	if (reservoir->bitCount & 7) {
		mp3ReservoirRead(reservoir, 8 - (reservoir->bitCount & 7));
		position++;
	}
	available = decoder->mainDataFill - decoder->side.mainDataBegin - position;
	decoder->mainDataFill += mainData;
	if (available < 0)
		return MP3_STATUS_RESERVOIR;
	if (position > RESERVOIR_WRAP) {
		decoder->mainDataFill -= RESERVOIR_WRAP;
		reservoir->bitCount -= RESERVOIR_WRAP * 8;
		reservoir->readIndex -= MP3_RESERVOIR_WORDS;
	}
	if (available) {
		reservoir->bitCount += available * 8;
		reservoir->readIndex += available >> 2;
		if (available & 3) {
			position = reservoir->bitOffset + (available & 3) * 8;
			if (position >= 32) {
				position -= 32;
				reservoir->readIndex++;
			}
			reservoir->bitOffset = position;
		}
	}

	for (gr = 0; gr < granules; gr++) {
		for (ch = 0; ch < channels; ch++) {
			decoder->granuleStart = reservoir->bitCount;
			if (!header->id)
				part2 = lsfScalefactors(decoder, ch, gr);
			else
				part2 = scalefactors(decoder, ch, gr);
			result = mp3Huffman(decoder, ch, gr, part2, decoder->shortWidths,
				decoder->longBounds);
			if (result)
				return result;
		}
		for (ch = 0; ch < channels; ch++)
			mp3Requantize(decoder, ch, gr);
		if (channels == 2 && header->mode == MP3_MODE_JOINT_STEREO)
			stereo(decoder, gr);

		synthesis = decoder->synthesis;
		for (ch = 0; ch < channels; ch++) {
			granule = &decoder->side.channel[ch].granule[gr];
			synthesis->blockType[ch] = granule->blockType;
			synthesis->mixedBlock[ch] = granule->mixedBlock;
			synthesis->windowSwitching[ch] = granule->windowSwitching;
			synthesis->channels = channels;
			synthesis->samplingIndex = header->samplingIndex;
			synthesis->shortBounds = mp3BandBounds[synthesis->samplingIndex].shortBounds;
			/* subbands the antialiasing covers, and the long bands of a mixed block */
			if (synthesis->windowSwitching[ch] &&
			    synthesis->blockType[ch] == MP3_BLOCK_SHORT) {
				if (!synthesis->mixedBlock[ch])
					antialias = 0;
				else if (synthesis->samplingIndex == MP3_SAMPLING_8000)
					antialias = 4;
				else
					antialias = 2;
			} else {
				antialias = 32;
			}
			if (synthesis->windowSwitching[ch] && synthesis->mixedBlock[ch])
				longBands = synthesis->samplingIndex == MP3_SAMPLING_8000 ? 4 : 2;
			else
				longBands = 0;
			mp3Reorder(synthesis->xr[ch], synthesis->work, synthesis->shortBounds,
				antialias);
			if (antialias)
				mp3Antialias(synthesis->work, antialias - 1);
			mp3Imdct(synthesis->work, synthesis->xr[ch], synthesis->overlap[ch],
				synthesis->blockType[ch], synthesis->imdctWork, longBands);
		}
		for (ch = 0; ch < channels; ch++)
			mp3Polyphase(synthesis->xr[ch], synthesis->work, synthesis->polyphase[ch],
				synthesis->polyphaseOffset);
		for (ch = 0; ch < channels; ch++)
			mp3Output(synthesis->xr[ch], pointers[gr * 2 + ch], decoder->bytesPerSample,
				MP3_GRANULE_SAMPLES, stride, decoder->bigEndianOutput);
	}

	if (decoder->interleaved == 1 && decoder->channels && header->emphasis == 1 &&
	    header->id == 1) {
		if (decoder->emphasis != header->emphasis)
			emphasisReset(decoder, header->emphasis);
		if (decoder->channels == 1 || decoder->channels == 2)
			deemphasis(decoder->channels, header->samplingRate, (int16_t *)pointers[0],
				decoder);
	} else {
		decoder->staleFilter[3] = 0;
		decoder->staleFilter[2] = 0;
		decoder->staleFilter[1] = 0;
		decoder->staleFilter[0] = 0;
	}
	decoder->channels = channels;
	decoder->frames++;
	decoder->samples = granules * MP3_GRANULE_SAMPLES;
	return 0;
}

/* 0x869924: the next frame of the input */
int32_t mp3DecodeFrame(Mp3Decoder *decoder)
{
	int32_t available, left, result;

	if (!decoder)
		return MP3_STATUS_NO_DECODER;
	if (!decoder->input)
		return MP3_STATUS_NO_INPUT;
	if (!decoder->output || !decoder->outputSize)
		return MP3_STATUS_NO_OUTPUT;
	if (decoder->halted == 1)
		return MP3_STATUS_HALTED;
	if (decoder->bytesPerSample != 2 && decoder->bytesPerSample != 3)
		decoder->bytesPerSample = 2;
	decoder->status = 0;
	decoder->warning = 0;
	decoder->samples = 0;
	decoder->outputBytes = decoder->bytesPerSample;
	result = frameSync(decoder, &available, &left);
	if (!result) {
		if (decoder->header.layer != 3)
			return MP3_STATUS_LAYER;
		result = frameDecode(decoder);
	}
	if (!result) {
		if (decoder->needSync)
			decoder->needSync = 0;
		decoder->lastFrameSize = decoder->header.frameSize;
	}
	return result;
}
