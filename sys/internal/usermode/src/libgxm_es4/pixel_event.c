/*
 * SceGxm: pixel event programs.
 *
 * The USSE programs the PDS starts at the end of a tile (the colour surface
 * write), at the partial terminate and at the end of a render, and the PDS
 * program that selects between them. The driver writes their code.
 */

#include "gxm_internal.h"

/* the most bytes each program takes (Sony's assertions name them) */
#define USE_PIXELEVENT_EOT_BYTES	0x40
#define USE_PIXELEVENT_PTOFF_BYTES	8
#define USE_PIXELEVENT_EOR_BYTES	0x20
#define PDS_PIXEVENT_PROG_SIZE		0x90

int gxmPdsBuildDoutProgram(GxmChunkBuffer *buffer, const SceGxmColorSurface *surface,
			   GxmCodeRef *program)
{
	SceUInt32 words[6], offset, usseDataEnd;
	void *usseData = gxmChunkBufferReserve(buffer, USE_PIXELEVENT_EOT_BYTES, 0x10);

	if (usseData == NULL) {
		GXM_DBG_ERROR(35, "Could not reserve %d bytes.", USE_PIXELEVENT_EOT_BYTES);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	words[0] = surface->format;
	words[1] = surface->data;
	words[2] = surface->stride;
	words[3] = surface->reserved;
	offset = gxmChunkBufferOffset(buffer);
	words[4] = surface->clipMin;
	words[5] = surface->clipMax;
	usseDataEnd = SceGpuEs4User_53CACDAB(usseData, words, surface->control);
	GXM_DBG_ASSERT(46, (uint32_t)usseDataEnd - (uint32_t)usseData <= USE_PIXELEVENT_EOT_BYTES);
	gxmChunkBufferCommit(buffer, (void *)usseDataEnd);
	program->address = 0;
	program->taskControl = 0;
	gxmCodeRefSetAddress(program, offset);
	return 0;
}

int gxmPdsBuildNopProgram(GxmChunkBuffer *buffer, GxmCodeRef *program)
{
	void *usseData = gxmChunkBufferReserve(buffer, USE_PIXELEVENT_PTOFF_BYTES, 0x10);
	SceUInt32 offset, usseDataEnd;

	if (usseData == NULL) {
		GXM_DBG_ERROR(70, "Could not reserve %d bytes.", USE_PIXELEVENT_PTOFF_BYTES);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	offset = gxmChunkBufferOffset(buffer);
	usseDataEnd = SceGpuEs4User_1A575DFD(usseData);
	GXM_DBG_ASSERT(76, (uint32_t)usseDataEnd - (uint32_t)usseData <= USE_PIXELEVENT_PTOFF_BYTES);
	gxmChunkBufferCommit(buffer, (void *)usseDataEnd);
	program->address = 0;
	program->taskControl = 0;
	gxmCodeRefSetAddress(program, offset);
	return 0;
}

int gxmPdsBuildKickProgram(GxmChunkBuffer *buffer, GxmCodeRef *program)
{
	void *usseData = gxmChunkBufferReserve(buffer, USE_PIXELEVENT_EOR_BYTES, 0x10);
	SceUInt32 offset, usseDataEnd;

	if (usseData == NULL) {
		GXM_DBG_ERROR(100, "Could not reserve %d bytes.", USE_PIXELEVENT_EOR_BYTES);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	offset = gxmChunkBufferOffset(buffer);
	usseDataEnd = SceGpuEs4User_86FC3202(usseData);
	GXM_DBG_ASSERT(106, (uint32_t)usseDataEnd - (uint32_t)usseData <= USE_PIXELEVENT_EOR_BYTES);
	gxmChunkBufferCommit(buffer, (void *)usseDataEnd);
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
			    GxmCommandBlock *eventPdsProgram, GxmPdsState *state)
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
	code = gxmChunkBufferReserve(buffer, PDS_PIXEVENT_PROG_SIZE, 0x10);
	if (code == NULL) {
		GXM_DBG_ERROR(151, "Could not reserve %d bytes.", PDS_PIXEVENT_PROG_SIZE);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	eventPdsProgram->data = code;
	count = (mode == 1 && samples == 1) ? 4 : samples;
	end = SceGpuEs4User_E98914E4(&params, code, samples != 1 && mode == 0,
				     mode == 1 && samples == 1, count);
	eventPdsProgram->dataSize = params.dataSize;
	eventPdsProgram->totalSize = end - (SceUInt32)eventPdsProgram->data;
	GXM_DBG_ASSERT(168, eventPdsProgram->totalSize <= PDS_PIXEVENT_PROG_SIZE);
	gxmChunkBufferCommit(buffer, (void *)end);
	state->code = eventPdsProgram->data;
	state->dataSize = eventPdsProgram->dataSize >> 4;
	state->flags = 0x4000;
	return 0;
}
