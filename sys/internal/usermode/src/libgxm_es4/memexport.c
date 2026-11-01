/*
 * SceGxm: the library's own GPU programs.
 */

#include "gxm_internal.h"

/* a VDM block that runs a USSE program through the PDS */
int gxmWriteProgramBlock(GxmChunkBuffer *buffer, const GxmCodeRef *usseProgram,
			 GxmCommandBlock *pdsProgram)
{
	SceUInt32 *words;

	GXM_DBG_ASSERT(57, buffer);
	GXM_DBG_ASSERT(58, usseProgram);
	GXM_DBG_ASSERT(59, pdsProgram);
	words = gxmChunkBufferReserve(buffer, 0x18, 0x10);
	if (words == NULL) {
		GXM_DBG_ERROR(73, "Could not reserve %d bytes.", 0x18);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	pdsProgram->dataSize = 0x10;
	pdsProgram->data = words;
	pdsProgram->totalSize = 0x18;
	words[0] = usseProgram->address;
	words[1] = usseProgram->taskControl;
	words[2] = 0;
	words[3] = 0;
	words[4] = gxmPdsDataWord(5, 0, 3);
	words[5] = 0xaf000000;
	gxmChunkBufferCommit(buffer, words + 6);
	return 0;
}

/* the program that writes the fence word */
int gxmBuildFenceProgram(GxmChunkBuffer *buffer, GxmCodeRef *program, SceUInt32 fenceAddress)
{
	SceUInt32 *code = gxmChunkBufferReserve(buffer, GXM_FENCE_PROGRAM_SIZE, 0x10);
	SceUInt32 offset;

	if (code == NULL) {
		GXM_DBG_ERROR(109, "Could not reserve %d bytes.", GXM_FENCE_PROGRAM_SIZE);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	offset = buffer->cur + buffer->gpuEnd - buffer->end;
	code[0] = 0x00000000;
	code[1] = 0xfa440700;
	code[2] = 0x00000070;
	code[3] = 0xfd200000;
	code[4] = 0x2011405f;
	code[5] = 0xfea10000;
	code[6] = 0x00000000;
	code[7] = 0xf9200000;
	code[8] = 0xa0000081;
	code[9] = 0xa8830080;
	code[10] = 0x2011405f;
	code[11] = 0xfea90000;
	code[12] = 0x20078010;
	code[13] = 0x48890281;
	code[14] = 0x00000010;
	code[15] = 0xf9000040;
	code[16] = 0xa0010102;
	code[17] = 0xfeab0000;
	code[18] = 0x00200000;
	code[19] = 0xfca01000;
	code[20] = 0x2000004b;
	code[21] = 0xfea10000;
	code[22] = 0x00000000;
	code[23] = 0xf9200000;
	code[24] = 0x000c0001;
	code[25] = 0x48880181;
	code[26] = 0x000ffffd;
	code[27] = 0xf9000040;
	code[28] = 0x200000cd;
	code[29] = 0xfea90000;
	code[30] = 0xa0010104;
	code[31] = 0xfeab0000;
	code[32] = 0x00200080;
	code[33] = 0xfca00000;
	code[34] = 0x20000046;
	code[35] = 0xfea10000;
	code[36] = 0x00000000;
	code[37] = 0xf9200000;
	code[38] = 0x000c0001;
	code[39] = 0x48880181;
	code[40] = 0x000ffffd;
	code[41] = 0xf9000040;
	code[42] = 0x200000c5;
	code[43] = 0xfea90000;
	code[44] = 0xa011405f;
	code[45] = 0xfeab0000;
	code[46] = 0x2011405f;
	code[47] = 0xfea10000;
	code[48] = 0x00000000;
	code[49] = 0xf9200000;
	code[50] = 0x00000071;
	code[51] = 0xfd200000;
	code[52] = fenceAddress & 0x1fffff;
	code[53] = ((fenceAddress >> 17) & 0x1f0) | (fenceAddress >> 26) << 12 | 0xfca00000;
	code[54] = 0xa0000001;
	code[55] = 0xf0a30020;
	code[56] = 0x00000000;
	code[57] = 0xf8a00000;
	code[58] = 0x00000000;
	code[59] = 0xf9200000;
	code[60] = 0xa00840e4;
	code[61] = 0xfeab0000;
	code[62] = 0x00000000;
	code[63] = 0xf8040140;
	gxmChunkBufferCommit(buffer, code + 64);
	program->address = 0;
	program->taskControl = 0;
	gxmCodeRefSetAddress(program, offset);
	return 0;
}
