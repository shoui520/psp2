#include "atrac_internal.h"

/* KSID_ATRAC9 subformat GUID of an ATRAC9 WAVE_FORMAT_EXTENSIBLE header */
static const SceUChar8 g_at9SubFormat[16] = {
	0xd2, 0x42, 0xe1, 0x47, 0xba, 0x36, 0x8d, 0x4d,
	0x88, 0xfc, 0x61, 0x65, 0x4f, 0x8c, 0x83, 0x6c,
};

/* ---------------------------------------------------------------------- */
/* RIFF/WAVE header                                                       */

#define ATRAC_FOURCC(a, b, c, d)	((a) | (b) << 8 | (c) << 16 | (d) << 24)
#define ATRAC_CHUNK_RIFF		ATRAC_FOURCC('R', 'I', 'F', 'F')
#define ATRAC_CHUNK_WAVE		ATRAC_FOURCC('W', 'A', 'V', 'E')
#define ATRAC_CHUNK_FMT			ATRAC_FOURCC('f', 'm', 't', ' ')
#define ATRAC_CHUNK_FACT		ATRAC_FOURCC('f', 'a', 'c', 't')
#define ATRAC_CHUNK_SMPL		ATRAC_FOURCC('s', 'm', 'p', 'l')
#define ATRAC_CHUNK_DATA		ATRAC_FOURCC('d', 'a', 't', 'a')
#define ATRAC_WAVE_FORMAT_EXTENSIBLE	0xfffe

/* little endian value of size bytes */
static inline SceUInt32 atracReadLe(const SceUChar8 *p, int size)
{
	SceUInt32 value = 0;
	SceUInt32 shift = size * 8;

	p += size;
	do {
		shift -= 8;
		value |= *--p << shift;
	} while (--size != 0);
	return value;
}

SceInt32 atracParseHeader(AtracHeader *header, const SceUChar8 *buffer, SceUInt32 size)
{
	SceInt32 ret = SCE_ATRAC_ERROR_UNSUPPORTED_DATA;
	SceUInt32 offset = 0, need = ATRAC_RIFF_HEADER_SIZE;
	SceUInt32 riffSize, pos, id, length, rest, data, loops, channels;
	const SceUChar8 *fmt;

	header->atracType = 0;
	header->channels = 0;
	header->blockAlign = 0;
	header->dataOffset = 0;
	header->dataSize = 0;
	header->totalSamples = 0;
	header->delaySamples = 0;
	header->loopStart = -1;
	header->loopEnd = -1;

	/* the RIFF chunk of form WAVE */
	for (;;) {
		if (size < need)
			return SCE_ATRAC_ERROR_READ_SIZE_IS_TOO_SMALL;
		if (atracReadLe(buffer + offset, 4) != ATRAC_CHUNK_RIFF)
			return SCE_ATRAC_ERROR_UNSUPPORTED_DATA;
		riffSize = atracReadLe(buffer + offset + 4, 4);
		pos = offset + ATRAC_RIFF_HEADER_SIZE;
		if (atracReadLe(buffer + offset + 8, 4) == ATRAC_CHUNK_WAVE)
			break;
		offset = riffSize - 4 + ((riffSize - 4) & 1) + pos;
		if (size < offset)
			return SCE_ATRAC_ERROR_READ_SIZE_IS_TOO_SMALL;
		need = offset + ATRAC_RIFF_HEADER_SIZE;
	}

	if (offset + 0x14 >= size)
		return SCE_ATRAC_ERROR_READ_SIZE_IS_TOO_SMALL;
	do {
		id = atracReadLe(buffer + pos, 4);
		data = pos + 8;
		length = atracReadLe(buffer + pos + 4, 4);
		length += length & 1;
		if (size < length + data && id != ATRAC_CHUNK_DATA)
			return SCE_ATRAC_ERROR_READ_SIZE_IS_TOO_SMALL;
		rest = length;
		if (id == ATRAC_CHUNK_FACT) {
			if (length < 12)
				return SCE_ATRAC_ERROR_UNSUPPORTED_DATA;
			header->totalSamples = atracReadLe(buffer + pos + 8, 4);
			data = pos + 0x14;
			header->delaySamples = atracReadLe(buffer + pos + 0x10, 4);
			rest = length - 12;
		} else if (id == ATRAC_CHUNK_SMPL) {
			if (header->loopStart < 0) {
				if (length < 0x20)
					return SCE_ATRAC_ERROR_UNSUPPORTED_DATA;
				rest = length - 0x20;
				data = pos + 0x28;
				loops = atracReadLe(buffer + pos + 0x24, 4);
				if (loops != 0) {
					if (rest < 0x14)
						return SCE_ATRAC_ERROR_UNSUPPORTED_DATA;
					rest = length - 0x34;
					header->loopStart = atracReadLe(buffer + pos + 0x34, 4);
					data = pos + 0x3c;
					header->loopEnd = atracReadLe(buffer + pos + 0x38, 4);
					if (header->loopEnd < header->loopStart)
						return SCE_ATRAC_ERROR_INVALID_DATA;
					if (header->loopEnd < header->loopStart +
					    SCE_ATRAC_AT9_MIN_LOOP_SAMPLES - 1)
						return SCE_ATRAC_ERROR_UNSUPPORTED_DATA;
				}
			}
		} else if (id == ATRAC_CHUNK_DATA) {
			header->dataOffset = data;
			header->dataSize = length;
			return ret;
		} else if (id == ATRAC_CHUNK_FMT) {
			if (ret != SCE_ATRAC_ERROR_UNSUPPORTED_DATA || length < 0x34)
				return SCE_ATRAC_ERROR_UNSUPPORTED_DATA;
			fmt = buffer + pos;
			if (atracReadLe(fmt + 8, 2) != ATRAC_WAVE_FORMAT_EXTENSIBLE)
				return SCE_ATRAC_ERROR_UNSUPPORTED_DATA;
			header->channels = atracReadLe(fmt + 0xa, 2);
			if (header->channels == 0 || header->channels > SCE_ATRAC_AT9_MAX_CH_IN_DECODER)
				return SCE_ATRAC_ERROR_UNSUPPORTED_DATA;
			header->samplingRate = atracReadLe(fmt + 0xc, 4);
			if (header->samplingRate != 48000 && header->samplingRate != 24000 &&
			    header->samplingRate != 12000)
				return SCE_ATRAC_ERROR_UNSUPPORTED_DATA;
			header->blockAlign = atracReadLe(fmt + 0x14, 2);
			if (header->blockAlign == 0)
				return SCE_ATRAC_ERROR_UNSUPPORTED_DATA;
			header->blockSamples = atracReadLe(fmt + 0x1a, 2);
			if (compareBytes(buffer + pos + 0x20, g_at9SubFormat, sizeof(g_at9SubFormat)) != 0)
				return SCE_ATRAC_ERROR_UNSUPPORTED_DATA;
			copyBytes(header->configData, buffer + pos + 0x34, sizeof(header->configData));
			/* channel config of the ATRAC9 config word: 0 mono, 1-2 stereo */
			channels = (SceUInt32)(header->configData[1] << 28) >> 29;
			if (channels == 0)
				channels = 1;
			else
				channels = channels < 3 ? 2 : 0;
			if (channels != header->channels)
				return SCE_ATRAC_ERROR_UNSUPPORTED_DATA;
			header->atracType = SCE_ATRAC_TYPE_AT9;
			data = pos + 0x3c;
			ret = 0;
			rest = length - 0x34;
		}
		pos = rest + data;
	} while (pos <= size && pos + 8 < size);
	return SCE_ATRAC_ERROR_READ_SIZE_IS_TOO_SMALL;
}
