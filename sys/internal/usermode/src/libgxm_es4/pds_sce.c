/*
 * SceGxm: PDS programs.
 *
 * The PDS runs before each USSE program to fetch its inputs: vertex
 * attributes from the streams, fragment iterators and textures. These
 * functions size and write those programs; the words are hardware
 * encodings and are kept as numbers.
 */

#include "gxm_internal.h"

#define SCE_GXM_PDS_ALIGNMENT	16

/* the MOVS destinations a data word may not name */
#define EURASIA_PDS_MOVS_DEST_DOUTI	2
#define EURASIA_PDS_MOVS_DEST_DOUTT	4

/* DOUTD control words, by register bank and word count - 1 */
static const SceUInt32 gxmPdsDoutWords[4][4] = {
	{ 0x07000000, 0x07000100, 0x07000180, 0x070001b0 },
	{ 0x07000550, 0x07000650, 0x070006d0, 0x00000000 },
	{ 0x07000aa0, 0x07000ba0, 0x07000b20, 0x07000b10 },
	{ 0x07000ff0, 0x07000cf0, 0x07000c70, 0x00000000 },
};

/*
 * Copy whole words: 16 bytes first when the source is not 32-byte aligned, then 32-byte
 * blocks through the core and NEON registers, then single words. Sony's is hand-written
 * ARM, inlined into its callers and kept out of line as well.
 */
void gxmCopyWords(SceUInt32 *dest, const SceUInt32 *src, SceSize size)
{
	GXM_DBG_ASSERT(21, SCE_GXM_PTR_IS_ALIGNED(dest, SCE_GXM_PDS_ALIGNMENT));
	GXM_DBG_ASSERT(22, SCE_GXM_PTR_IS_ALIGNED(src, SCE_GXM_PDS_ALIGNMENT));
	GXM_DBG_ASSERT(23, size >= 16);
	__asm__ volatile(
		"movs	r3, %1, lsl #27\n"
		"ldmne	%1!, {r3-r6}\n"
		"stmne	%0!, {r3-r6}\n"
		"subne	%2, %2, #16\n"
		"subs	%2, %2, #32\n"
		"blt	2f\n"
		"1:\n"
		"ldm	%1!, {r3-r6}\n"
		"pld	[%1, #0x70]\n"
		"vldm	%1!, {d0-d1}\n"
		"subs	%2, %2, #32\n"
		"stm	%0!, {r3-r6}\n"
		"vstm	%0!, {d0-d1}\n"
		"bge	1b\n"
		"2:\n"
		"adds	%2, %2, #32\n"
		"beq	4f\n"
		"3:\n"
		"ldr	r3, [%1], #4\n"
		"subs	%2, %2, #4\n"
		"str	r3, [%0], #4\n"
		"bne	3b\n"
		"4:\n"
		: "+r" (dest), "+r" (src), "+r" (size)
		:
		: "r3", "r4", "r5", "r6", "d0", "d1", "cc", "memory");
}

/* the PDS state words of a program pair */
void gxmPdsWriteStateWords(SceUInt32 *words, const GxmCommandBlock *vertex, SceUInt32 flags,
			   const GxmCommandBlock *fragment)
{
	SceUInt32 vertexSize = vertex->dataSize;
	SceUInt32 fragmentSize = fragment->dataSize;
	SceUInt32 vertexAddress = (SceUInt32)vertex->data;
	SceUInt32 fragmentAddress = (SceUInt32)fragment->data;

	words[0] = (vertexAddress << 1) >> 5 | (vertexSize >> 4) << 27;
	words[1] = ((SceInt32)vertexAddress < 0 ? 0x4000 : 0) | flags |
		   ((SceInt32)fragmentAddress < 0 ? 0x2000 : 0);
	words[2] = (fragmentAddress << 1) >> 5 | (fragmentSize >> 4) << 27;
}

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

/* copy a command block into the chunk buffer */
int gxmCommandBlockCopy(GxmChunkBuffer *buffer, const GxmCommandBlock *source, GxmCommandBlock *copy)
{
	SceSize size = source->totalSize;
	SceUInt32 *data = gxmChunkBufferReserve(buffer, size, 0x10);

	if (data == NULL) {
		GXM_DBG_ERROR(175, "Could not reserve %d bytes.", size);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	gxmChunkBufferCommit(buffer, (SceUInt8 *)data + size);
	copy->data = data;
	copy->dataSize = source->dataSize;
	copy->totalSize = size;
	gxmCopyWords(data, source->data, size);
	return 0;
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
	SceUInt32 *constBase, *instr, *next, reserveSize, dataSize, stream, dataIndex, i, odd, high;
	SceUInt32 dataOffset, programOffset, nextOffset, word, term, bank, flag, last, dmaSize;
	const SceGxmVertexAttribute *attribute;

	for (i = 0; i < attributeCount; i++) {
		if (attributes[i].streamIndex >= streamCount) {
			GXM_DBG_ERROR(225, "Attribute %u has a streamIndex of %u, which is out of bounds.",
				      i, attributes[i].streamIndex);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	}
	reserveSize = gxmPdsVertexProgramSize(attributeCount, streams, streamCount, &dataSize,
					      instanced, indexed);
	constBase = gxmChunkBufferReserve(buffer, reserveSize, 0x10);
	if (constBase == NULL) {
		GXM_DBG_ERROR(240, "Could not reserve %d bytes.", reserveSize);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	instr = (SceUInt32 *)((SceUInt32)constBase + (dataSize & ~3));
	gxmChunkBufferCommit(buffer, (SceUInt8 *)constBase + (reserveSize & ~3));
	block->dataSize = dataSize;
	block->data = constBase;
	block->totalSize = reserveSize;

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
		next = instr + 1;
		if (!(flag & 2)) {
			*instr = stream << 9 | GXM_PDS_FETCH_STREAM;
			if (flag & 1) {
				flag = 0x10000;
				goto divide;
			}
		} else {
			*instr = stream << 9 | GXM_PDS_FETCH_STREAM_INDEXED;
			if (flag & 1) {
				flag = 0x30000;
divide:
				next = instr + 4;
				instr[3] = 0x47606070;
				instr[2] = 0xf7301030;
				instr[1] = stream << 9 | flag | 0x67800030;
			}
		}
		constBase[gxmPdsDataSlot(stream)] = streams->stride;
		instr = next;

		/* then every attribute in it */
		for (i = 0, attribute = attributes; i < attributeCount; i++, attribute++) {
			if (attribute->streamIndex != stream)
				continue;
			dmaSize = (attribute->componentCount *
				   gxmAttributeFormatGetSize(attribute->format) + 3) >> 2;
			GXM_DBG_ASSERT(322, 0 < dmaSize && dmaSize <= EURASIA_PDS_DOUTD1_BSIZE_MAX);
			*instr++ = (dataIndex >> 1) << 18 | GXM_PDS_FETCH_ATTRIBUTE;
			if (patches != NULL) {
				patches[i].streamIndex = stream;
				patches[i].offset = attribute->offset;
				patches[i].dataWord = dataIndex << 1;
			}
			constBase[dataIndex * 2] = attribute->offset;
			constBase[dataIndex * 2 + 1] = (dmaSize - 1) | attribute->regIndex << 8;
			dataIndex += 2;
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
	*instr++ = word;
	constBase[nextOffset / 4] = instanceData << 8;
	nextOffset = gxmPdsDataSlot(last) << 2;
	high = (last >> 1) << 12;

finish:
	if (indexed) {
		last++;
		word = high | 0x07840006 | (odd == 0 ? 0x200 : 0x300) | (odd == 0 ? 0x20 : 0x30);
		odd = last & 1;
		*instr++ = word;
		high = (last >> 1) << 12;
		constBase[nextOffset / 4] = indexData << 8;
		nextOffset = gxmPdsDataSlot(last) << 2;
		instr[0] = word;
		instr[1] = word;
		instr += 2;
	} else if (instanced) {
		instr[0] = word;
		instr[1] = word;
		instr += 2;
	}
	*instr++ = high | term | (odd == 0 ? 0x80 : 0xc0) | (odd == 0 ? 0x20 : 0x30);
	constBase[dataOffset / 4] = program->address;
	constBase[programOffset / 4] = program->taskControl;
	constBase[nextOffset / 4] = 0;
	*instr++ = GXM_PDS_END;
	GXM_DBG_ASSERT(439, instr == constBase + reserveSize/4);
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

SceUInt32 gxmPdsDataWord(SceUInt32 dst, SceUInt32 slot, SceUInt32 count)
{
	SceUInt32 instr;

	GXM_DBG_ASSERT(639, dst != EURASIA_PDS_MOVS_DEST_DOUTI);
	GXM_DBG_ASSERT(640, dst != EURASIA_PDS_MOVS_DEST_DOUTT);
	GXM_DBG_ASSERT(641, (count > 0) && (count <= 4));
	instr = gxmPdsDoutWords[slot & 3][count - 1];
	GXM_DBG_ASSERT(690, instr != 0);
	return instr | dst | (slot >> 2) << 12 | ((slot >> 2) + ((slot >> 1) & 1)) << 18;
}

int gxmPdsBuildFragmentProgram(GxmChunkBuffer *buffer, const SceGxmTexture *textures,
			       const GxmProgramFragmentInput *inputs, SceUInt32 inputCount,
			       const SceUInt8 *texCoordRemap, const GxmCodeRef *program,
			       GxmCommandBlock *block, SceUInt32 *textureIndices)
{
	SceUInt32 *constBase, *instr, reserveSize, dataSize, iterator, slot, word, coord, uv, i;
	SceUInt32 dataOffset, programOffset, endOffset, high, low, sampler;
	const GxmProgramFragmentInput *input;
	const SceGxmTexture *texture;

	reserveSize = gxmPdsFragmentProgramSize(inputs, inputCount, &dataSize);
	constBase = gxmChunkBufferReserve(buffer, reserveSize, 0x10);
	if (constBase == NULL) {
		GXM_DBG_ERROR(727, "Could not reserve %d bytes.", reserveSize);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	instr = (SceUInt32 *)((SceUInt32)constBase + dataSize);
	gxmChunkBufferCommit(buffer, (SceUInt8 *)constBase + reserveSize);
	block->data = constBase;
	block->dataSize = dataSize;
	block->totalSize = reserveSize;

	if (inputCount == 0) {
		slot = 0;
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
				texture = &textures[inputs[i].textureIndex];
				constBase[slot + 0] = texture->controlWords[0];
				constBase[slot + 1] = texture->controlWords[1];
				constBase[slot + 2] = texture->controlWords[2];
				constBase[slot + 3] = texture->controlWords[3];
				slot += 4;
			}
		}
		dataOffset = slot * 4;
		programOffset = dataOffset + 4;
		endOffset = dataOffset + 8;
	}
	*instr++ = gxmPdsDataWord(5, slot, 3);
	slot += 3;
	constBase[dataOffset / 4] = program->address;
	constBase[programOffset / 4] = program->taskControl;
	constBase[endOffset / 4] = 0;

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
		*instr++ = word | input->iteratorBits;
		slot++;
		constBase[slot - 1] = iterator;
		if ((iterator & 0xf) == 0xf)
			continue;
		/* and the texture fetch */
		*instr++ = input->samplerBits | 0x07000004 | (sampler & 0x3fffffff) << 18 |
			  (sampler & 0x3fffffff) << 12;
		sampler++;
		if (textureIndices != NULL)
			*textureIndices++ = input->textureIndex;
	}
	*instr++ = GXM_PDS_END;
	GXM_DBG_ASSERT(817, instr == constBase + reserveSize/4);
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
	SceUInt32 reserveSize = words * 4;
	SceUInt32 dataSize = dataWords << 2;
	SceUInt32 *constBase, *instr, programOffset, taskControlOffset, endOffset, slot;

	constBase = gxmChunkBufferReserve(buffer, reserveSize, 0x10);
	if (constBase == NULL) {
		GXM_DBG_ERROR(875, "Could not reserve %d bytes.", reserveSize);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	instr = (SceUInt32 *)((SceUInt8 *)constBase + dataSize);
	gxmChunkBufferCommit(buffer, constBase + reserveSize / 4);
	block->data = constBase;
	block->dataSize = dataSize;
	block->totalSize = reserveSize;
	if (extra) {
		*instr++ = 0x07000103;
		constBase[0] = value;
		constBase[1] = count - 1;
		slot = 2;
		endOffset = 0x10;
		taskControlOffset = 0xc;
		programOffset = 8;
	} else {
		slot = 0;
		endOffset = 8;
		taskControlOffset = 4;
		programOffset = 0;
	}
	*instr++ = gxmPdsDataWord(5, slot, 3);
	constBase[programOffset / 4] = program->address;
	constBase[taskControlOffset / 4] = program->taskControl;
	constBase[endOffset / 4] = 0;
	*instr++ = GXM_PDS_END;
	GXM_DBG_ASSERT(910, instr == constBase + reserveSize/4);
	return 0;
}
