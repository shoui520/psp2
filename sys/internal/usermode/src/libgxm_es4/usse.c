/*
 * SceGxm: USSE program pieces.
 *
 * Small fixed programs and the words every generated program starts with.
 * The instruction words are kept as the hardware encodes them.
 */

#include "gxm_internal.h"

/* a code reference holds the program's offset in the USSE heap */
void gxmCodeRefSetAddress(GxmCodeRef *ref, SceUInt32 offset)
{
	ref->address = (ref->address & 0xff000000) | offset >> 24 | ((offset << 9) >> 12) << 4;
}

/* the start of a program for older SDKs: three instruction pairs */
SceUInt32 *gxmUsseWriteOldHeader(SceUInt32 *code)
{
	code[0] = 0xa00c0000;
	code[1] = 0x48830282;
	code[2] = 0xa0000000;
	code[3] = 0xe9833084;
	code[4] = 0x00000000;
	code[5] = 0xf9200000;
	return code + 6;
}

/* the start of a program: a phase instruction, then the same three pairs */
SceUInt32 *gxmUsseWriteHeader(SceUInt32 *usseCode)
{
	GXM_DBG_ASSERT(98, ((uintptr_t)usseCode & 0xf) == 0x8);
	usseCode[0] = 0x00000000;
	usseCode[1] = 0xf8000940;
	usseCode[2] = 0xa00c0000;
	usseCode[3] = 0x48830282;
	usseCode[4] = 0xa0000000;
	usseCode[5] = 0xe9833084;
	usseCode[6] = 0x00000000;
	usseCode[7] = 0xf9200000;
	return usseCode + 8;
}

SceSize gxmUsseGetEmptyProgramSize(void)
{
	if (gxmGlobals.compatFlags & GXM_COMPAT_USSE_OLD_HEADER)
		return 0x28;
	return 0x50;
}

/* a program that does nothing but end */
int gxmUsseBuildEmptyProgram(GxmChunkBuffer *buffer, GxmCodeRef *program)
{
	_Bool old = (gxmGlobals.compatFlags & GXM_COMPAT_USSE_OLD_HEADER) != 0;
	SceSize alignment = old ? 8 : 16;
	SceSize size = old ? 0x28 : 0x50;
	SceUInt32 *code, *out, offset;

	code = gxmChunkBufferReserve(buffer, size, alignment);
	if (code == NULL) {
		GXM_DBG_ERROR(148, "Could not reserve %d bytes.", size);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	offset = buffer->current + buffer->gpuEnd - buffer->end;
	code[1] = 0xfa440700;
	code[0] = 0;
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_USSE_OLD_HEADER))
		out = gxmUsseWriteHeader(code + 2);
	else
		out = gxmUsseWriteOldHeader(code + 2);
	out[1] = 0xf8040140;
	out[0] = 0;
	gxmChunkBufferCommit(buffer, out + 2);
	program->address = offset >> 24 | ((offset << 9) >> 12) << 4;
	program->taskControl = 2;
	return 0;
}
