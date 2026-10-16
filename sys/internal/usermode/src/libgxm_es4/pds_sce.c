/*
 * SceGxm: PDS programs.
 *
 * The PDS runs before each USSE program to fetch its inputs: vertex
 * attributes from the streams, fragment iterators and textures. These
 * functions size and write those programs; the words are hardware
 * encodings and are kept as numbers.
 */

#include "gxm_internal.h"

/* DOUTD control words, by register bank and data size */
static const SceUInt32 gxmPdsDoutWords[4][4] = {
	{ 0x00000000, 0x07000000, 0x07000100, 0x07000180 },
	{ 0x070001b0, 0x07000550, 0x07000650, 0x070006d0 },
	{ 0x00000000, 0x07000aa0, 0x07000ba0, 0x07000b20 },
	{ 0x07000b10, 0x07000ff0, 0x07000cf0, 0x07000c70 },
};

/* where data word n lives: pairs, interleaved after the first two */
static inline SceUInt32 gxmPdsDataSlot(SceUInt32 n)
{
	return (n & 1) | 2 | (n & ~1) << 1;
}

/* the size of a vertex PDS program; *dataSize gets the data part */
GXM_NOINLINE SceSize gxmPdsVertexProgramSize(SceUInt32 attributeCount, const SceGxmVertexStream *streams,
				SceUInt32 streamCount, SceSize *dataSize, SceUInt8 instanced,
				SceUInt8 indexed)
{
	SceUInt32 code, dataWords, attributeData, a, b, data, i;

	code = attributeCount + 2;
	for (i = 0; i < streamCount; i++)
		code += (streams[i].indexSource & 1) ? 4 : 1;
	if (instanced) {
		code++;
		dataWords = streamCount + 2;
	} else {
		dataWords = streamCount + 1;
	}
	attributeData = (attributeCount + 1) * 2;
	if (indexed) {
		code++;
		dataWords++;
		code += 2;
	} else if (instanced) {
		code += 2;
	}
	a = 0;
	if (attributeData != 0)
		a = (attributeData - 2) * 2 | 1;
	b = 0;
	if (dataWords != 0)
		b = (((dataWords - 1) & 1) | 2) | ((dataWords - 1) & ~1) << 1;
	if (b < a)
		b = a;
	data = (b + 4) & ~3;
	if (dataSize != NULL)
		*dataSize = data << 2;
	return (data + code) * 4;
}

#define GXM_PDS_FETCH_STREAM		0x67800070
#define GXM_PDS_FETCH_STREAM_INDEXED	0x67820070
#define GXM_PDS_FETCH_ATTRIBUTE		0x2f0181a3
#define GXM_PDS_END			0xaf000000

int gxmPdsBuildVertexProgram(GxmChunkBuffer *buffer, const SceGxmVertexAttribute *attributes,
			     SceUInt32 attributeCount, const SceGxmVertexStream *streams,
			     SceUInt32 streamCount, const GxmCodeRef *program,
			     GxmCommandBlock *block, GxmPdsAttributePatch *patches,
			     SceUInt8 instanced, SceUInt8 indexed, SceUInt32 instanceData,
			     SceUInt32 indexData)
{
	SceUInt32 *data, *code, *next, size, dataSize, stream, dataIndex, i, odd, high;
	SceUInt32 dataOffset, programOffset, nextOffset, word, term, bank, flag, last;
	const SceGxmVertexAttribute *attribute;

	for (i = 0; i < attributeCount; i++) {
		if (attributes[i].streamIndex >= streamCount)
			return SCE_GXM_ERROR_INVALID_VALUE;
	}
	size = gxmPdsVertexProgramSize(attributeCount, streams, streamCount, &dataSize, instanced,
				       indexed);
	data = gxmChunkBufferReserve(buffer, size, 0x10);
	if (data == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	buffer->cur = (SceUInt32)data + (size & ~3);
	block->data = data;
	block->size = size;
	block->dataSize = dataSize;
	code = (SceUInt32 *)((SceUInt32)data + (dataSize & ~3));

	last = streamCount;
	if (streamCount == 0) {
		programOffset = 4;
		term = 0x03000105;
		nextOffset = 8;
		high = 0;
		dataOffset = 0;
		odd = 0;
		if (!instanced)
			goto finish;
		word = 0x20;
		bank = 0x200;
		goto instance;
	}

	dataIndex = 0;
	for (stream = 0; stream < streamCount; stream++, streams++) {
		/* fetch the stream's index (and divide it for instancing) */
		flag = streams->indexSource;
		next = code + 1;
		if (!(flag & 2)) {
			*code = stream << 9 | GXM_PDS_FETCH_STREAM;
			if (flag & 1) {
				flag = 0x10000;
				goto divide;
			}
		} else {
			*code = stream << 9 | GXM_PDS_FETCH_STREAM_INDEXED;
			if (flag & 1) {
				flag = 0x30000;
divide:
				next = code + 4;
				code[3] = 0x47606070;
				code[2] = 0xf7301030;
				code[1] = stream << 9 | flag | 0x67800030;
			}
		}
		data[gxmPdsDataSlot(stream)] = streams->stride;
		code = next;

		/* then every attribute in it */
		if (attributeCount == 0)
			continue;
		if (patches == NULL) {
			for (i = 0, attribute = attributes; i < attributeCount; i++, attribute++) {
				if (attribute->streamIndex != stream)
					continue;
				*code++ = (dataIndex >> 1) << 18 | GXM_PDS_FETCH_ATTRIBUTE;
				data[dataIndex * 2] = attribute->offset;
				data[dataIndex * 2 + 1] =
					((attribute->componentCount *
					  gxmAttributeFormatGetSize(attribute->format) + 3) >> 2) - 1 |
					attribute->regIndex << 8;
				dataIndex += 2;
			}
		} else {
			for (i = 0, attribute = attributes; i < attributeCount; i++, attribute++) {
				if (attribute->streamIndex != stream)
					continue;
				*code++ = (dataIndex >> 1) << 18 | GXM_PDS_FETCH_ATTRIBUTE;
				patches[i].streamIndex = stream;
				patches[i].offset = attribute->offset;
				patches[i].dataWord = dataIndex << 1;
				data[dataIndex * 2] = attribute->offset;
				data[dataIndex * 2 + 1] =
					((attribute->componentCount *
					  gxmAttributeFormatGetSize(attribute->format) + 3) >> 2) - 1 |
					attribute->regIndex << 8;
				dataIndex += 2;
			}
		}
	}
	odd = streamCount & 1;
	high = (streamCount >> 1) << 12;
	dataOffset = dataIndex * 8;
	nextOffset = gxmPdsDataSlot(streamCount) << 2;
	term = (dataIndex >> 1) << 18 | 0x03000105;
	programOffset = dataOffset + 4;
	if (!instanced)
		goto finish;
	if (odd == 0) {
		word = 0x20;
		bank = 0x200;
	} else {
		word = 0x30;
		bank = 0x300;
	}

instance:
	last = streamCount + 1;
	word = high | 0x07800006 | bank | word;
	odd = last & 1;
	*code++ = word;
	data[nextOffset / 4] = instanceData << 8;
	nextOffset = gxmPdsDataSlot(last) << 2;
	high = (last >> 1) << 12;

finish:
	if (indexed) {
		last++;
		word = high | 0x07840006 | (odd == 0 ? 0x200 : 0x300) | (odd == 0 ? 0x20 : 0x30);
		odd = last & 1;
		*code++ = word;
		high = (last >> 1) << 12;
		data[nextOffset / 4] = indexData << 8;
		nextOffset = gxmPdsDataSlot(last) << 2;
		code[0] = word;
		code[1] = word;
		code += 2;
	} else if (instanced) {
		code[0] = word;
		code[1] = word;
		code += 2;
	}
	*code = high | term | (odd == 0 ? 0x80 : 0xc0) | (odd == 0 ? 0x20 : 0x30);
	data[dataOffset / 4] = program->address;
	data[programOffset / 4] = program->taskControl;
	data[nextOffset / 4] = 0;
	code[1] = GXM_PDS_END;
	return 0;
}

/* point the attribute fetches at the streams' data */
void gxmPdsPatchAttributes(SceUInt32 *data, const GxmPdsAttributePatch *patches, SceUInt32 count,
			   const SceUInt32 *streamAddresses)
{
	SceUInt32 i;

	for (i = 0; i < count; i++)
		data[patches[i].dataWord] = streamAddresses[patches[i].streamIndex] + patches[i].offset;
}

void gxmPdsPatchAttributesPrecomputed(SceUInt32 *data, const GxmPdsAttributePatch *patches,
				      SceUInt32 count, const SceUInt32 *streamAddresses)
{
	SceUInt32 i;

	for (i = 0; i < count; i++)
		data[patches[i].dataWord] = streamAddresses[patches[i].streamIndex] + patches[i].offset;
}

/* point the fetches of one stream at its data */
void gxmPdsPatchStream(SceUInt32 *data, const GxmPdsAttributePatch *patches, SceUInt32 count,
		       SceUInt32 streamIndex, SceUInt32 address)
{
	SceUInt32 i;

	for (i = 0; i < count; i++) {
		if (patches[i].streamIndex == streamIndex)
			data[patches[i].dataWord] = patches[i].offset + address;
	}
}

/* the size of a fragment PDS program; *dataSize gets the data part */
SceSize gxmPdsFragmentProgramSize(const GxmProgramFragmentInput *inputs, SceUInt32 inputCount,
				  SceSize *dataSize)
{
	SceUInt32 code, data, i;
	SceSize size;

	if (inputCount == 0) {
		size = 0x18;
		data = 4;
	} else {
		data = 3;
		code = 2;
		for (i = 0; i < inputCount; i++) {
			if ((inputs[i].word & 0xf) == 0xf) {
				code += 1;
				data += 1;
			} else {
				code += 2;
				data += 5;
			}
		}
		data = (data + 3) & ~3;
		size = (data + code) * 4;
	}
	if (dataSize != NULL)
		*dataSize = data << 2;
	return size;
}

SceUInt32 gxmPdsDataWord(SceUInt32 base, SceUInt32 slot, SceUInt32 size)
{
	return gxmPdsDoutWords[slot & 3][size] | base | (slot >> 2) << 12 |
	       ((slot >> 2) + ((slot >> 1) & 1)) << 18;
}

int gxmPdsBuildFragmentProgram(GxmChunkBuffer *buffer, const SceGxmTexture *textures,
			       const GxmProgramFragmentInput *inputs, SceUInt32 inputCount,
			       const SceUInt8 *texCoordRemap, const GxmCodeRef *program,
			       GxmCommandBlock *block, SceUInt32 *textureIndices)
{
	SceUInt32 *data, *code, size, dataSize, words, iterator, slot, word, coord, uv, i, sampler;
	SceUInt32 dataOffset, programOffset, endOffset, high, low;
	const GxmProgramFragmentInput *input;

	if (inputCount == 0) {
		size = 0x18;
		dataSize = 0x10;
	} else {
		dataSize = 3;
		words = 2;
		for (i = 0; i < inputCount; i++) {
			if ((inputs[i].word & 0xf) == 0xf) {
				words += 1;
				dataSize += 1;
			} else {
				words += 2;
				dataSize += 5;
			}
		}
		dataSize = (dataSize + 3) & ~3;
		size = (dataSize + words) * 4;
		dataSize <<= 2;
	}
	data = gxmChunkBufferReserve(buffer, size, 0x10);
	if (data == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	buffer->cur = (SceUInt32)data + size;
	block->data = data;
	block->dataSize = dataSize;
	block->size = size;
	code = (SceUInt32 *)((SceUInt32)data + dataSize);

	if (inputCount == 0) {
		slot = 3;
		word = 0x07000185;
		endOffset = 8;
		programOffset = 4;
		dataOffset = 0;
	} else {
		/* the texture control words of every sampled input */
		slot = 0;
		if (textures == NULL) {
			for (i = 0; i < inputCount; i++) {
				if ((inputs[i].word & 0xf) != 0xf)
					slot += 4;
			}
		} else {
			for (i = 0; i < inputCount; i++) {
				if ((inputs[i].word & 0xf) == 0xf)
					continue;
				data[slot + 0] = textures[inputs[i].textureIndex].controlWords[0];
				data[slot + 1] = textures[inputs[i].textureIndex].controlWords[1];
				data[slot + 2] = textures[inputs[i].textureIndex].controlWords[2];
				data[slot + 3] = textures[inputs[i].textureIndex].controlWords[3];
				slot += 4;
			}
		}
		dataOffset = slot * 4;
		word = (slot >> 2) << 12 | (slot >> 2) << 18 | 0x07000185;
		slot += 3;
		programOffset = dataOffset + 4;
		endOffset = dataOffset + 8;
	}
	*code++ = word;
	data[dataOffset / 4] = program->address;
	data[programOffset / 4] = program->taskControl;
	data[endOffset / 4] = 0;

	sampler = 0;
	for (i = 0, input = inputs; i < inputCount; i++, input++) {
		/* the iterator, with its texture coordinates remapped */
		iterator = input->word;
		if (texCoordRemap != NULL) {
			uv = (iterator & 0xf000) >> 12;
			if (uv < 10)
				uv = texCoordRemap[uv];
			coord = iterator & 0xf;
			if (coord < 10)
				coord = texCoordRemap[coord];
			iterator = (iterator & 0xffff0ff0) | uv << 12 | coord;
		}
		high = slot >> 2;
		if (!(slot & 2)) {
			if (!(slot & 1))
				word = high << 12 | high << 18 | 0x07000002;
			else
				word = high << 12 | high << 18 | 0x07000502;
		} else {
			low = (slot & 1) ? 0xf02 : 0xa02;
			word = high << 12 | low | 0x07000000 | (high + 1) << 18;
		}
		*code++ = word | input->iteratorBits;
		slot++;
		data[slot - 1] = iterator;
		if ((iterator & 0xf) == 0xf)
			continue;
		/* and the texture fetch */
		*code++ = input->samplerBits | 0x07000004 | (sampler & 0x3fffffff) << 18 |
			  (sampler & 0x3fffffff) << 12;
		sampler++;
		if (textureIndices != NULL)
			*textureIndices++ = input->textureIndex;
	}
	*code = GXM_PDS_END;
	return 0;
}

/* a program with only the USSE jump, optionally preceded by two data words */
SceSize gxmPdsSimpleProgramSize(SceUInt32 count, SceSize *dataSize, SceSize unused)
{
	SceUInt32 data = (count * 2 + 6) & ~3;

	if (dataSize != NULL)
		*dataSize = data << 2;
	return (count + 2 + data) * 4;
}

int gxmPdsBuildSimpleProgram(GxmChunkBuffer *buffer, SceUInt32 value, SceUInt32 count,
			     const GxmCodeRef *program, GxmCommandBlock *block)
{
	_Bool extra = value != 0;
	SceUInt32 dataWords = extra ? 8 : 4;
	SceUInt32 words = extra + 2 + dataWords;
	SceUInt32 *data, *code, programOffset, taskControlOffset, endOffset, high;

	data = gxmChunkBufferReserve(buffer, words * 4, 0x10);
	if (data == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	buffer->cur = (SceUInt32)(data + words);
	code = data + dataWords;
	block->data = data;
	block->dataSize = dataWords << 2;
	block->size = words * 4;
	if (extra) {
		code++;
		data[dataWords] = 0x07000103;
		high = 0x40000;
		data[0] = value;
		data[1] = count - 1;
		endOffset = 0x10;
		taskControlOffset = 0xc;
		programOffset = 8;
	} else {
		endOffset = 8;
		taskControlOffset = 4;
		programOffset = 0;
		high = 0;
	}
	code[0] = gxmPdsDoutWords[extra ? 2 : 0][3] | 5 | high;
	data[programOffset / 4] = program->address;
	data[taskControlOffset / 4] = program->taskControl;
	data[endOffset / 4] = 0;
	code[1] = GXM_PDS_END;
	return 0;
}

/* code offset of the chunk buffer's write position */
static inline SceUInt32 gxmChunkBufferOffset(const GxmChunkBuffer *buffer)
{
	return buffer->cur + buffer->gpuEnd - buffer->end;
}

int gxmPdsBuildDoutProgram(GxmChunkBuffer *buffer, const GxmPdsDoutParams *params, GxmCodeRef *program)
{
	SceUInt32 words[6], offset;
	void *code = gxmChunkBufferReserve(buffer, 0x40, 0x10);

	if (code == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	words[0] = params->words[0];
	words[1] = params->words[1];
	words[2] = params->words[2];
	words[3] = params->words[3];
	offset = gxmChunkBufferOffset(buffer);
	words[4] = params->words[4];
	words[5] = params->words[5];
	buffer->cur = SceGpuEs4User_53CACDAB(code, words, params->count);
	program->address = 0;
	program->taskControl = 0;
	gxmCodeRefSetAddress(program, offset);
	return 0;
}

int gxmPdsBuildNopProgram(GxmChunkBuffer *buffer, GxmCodeRef *program)
{
	void *code = gxmChunkBufferReserve(buffer, 8, 0x10);
	SceUInt32 offset;

	if (code == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	offset = gxmChunkBufferOffset(buffer);
	buffer->cur = SceGpuEs4User_1A575DFD(code);
	program->address = 0;
	program->taskControl = 0;
	gxmCodeRefSetAddress(program, offset);
	return 0;
}

int gxmPdsBuildKickProgram(GxmChunkBuffer *buffer, GxmCodeRef *program)
{
	void *code = gxmChunkBufferReserve(buffer, 0x20, 0x10);
	SceUInt32 offset;

	if (code == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	offset = gxmChunkBufferOffset(buffer);
	buffer->cur = SceGpuEs4User_86FC3202(code);
	program->address = 0;
	program->taskControl = 0;
	gxmCodeRefSetAddress(program, offset);
	return 0;
}

typedef struct GxmPdsPixelParams {
	SceUInt32 program;		/* out: the aligned program address */
	SceUInt32 dataSize;		/* out */
	GxmCodeRef first;
	SceUInt32 firstFlags;
	GxmCodeRef second;
	SceUInt32 secondFlags;
	GxmCodeRef third;
	SceUInt32 thirdFlags;
} GxmPdsPixelParams;

int gxmPdsBuildPixelProgram(GxmChunkBuffer *buffer, SceUInt32 samples, SceUInt32 mode,
			    const GxmCodeRef *first, const GxmCodeRef *second, const GxmCodeRef *third,
			    GxmCommandBlock *block, GxmPdsState *state)
{
	GxmPdsPixelParams params;
	SceUInt32 end, count;
	void *code;

	params.second = *second;
	params.first = *first;
	params.third = *third;
	params.program = 0;
	params.dataSize = 0;
	params.secondFlags = 0;
	params.firstFlags = 0x20000;
	params.thirdFlags = 0x30000;
	code = gxmChunkBufferReserve(buffer, 0x90, 0x10);
	if (code == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	block->data = code;
	count = (mode == 1 && samples == 1) ? 4 : samples;
	end = SceGpuEs4User_E98914E4(&params, code, samples != 1 && mode == 0,
				     mode == 1 && samples == 1, count);
	block->dataSize = params.dataSize;
	block->size = end - (SceUInt32)block->data;
	buffer->cur = end;
	state->code = block->data;
	state->dataSize = block->dataSize >> 4;
	state->flags = 0x4000;
	return 0;
}
