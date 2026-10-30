/*
 * SceGpuEs4User: USSE (universal scalable shader engine) instruction emitters.
 *
 * Each USSE instruction is two words. The load sequence moves six addresses
 * into consecutive slots and ends with a word built from the count.
 *
 * PLACEHOLDER: the instruction encodings are reproduced as Sony's opcode
 * words and field shifts; the USSE instruction set is not yet decoded into
 * named fields.
 */

#include "gpu_es4_user.h"

#define USSE_LOAD_HEADER_LO	0x00000000
#define USSE_LOAD_HEADER_HI	0xfa440700
#define USSE_LOAD_SLOTS		6

/* one address load into a slot */
#define USSE_LOAD_LO(slot, addr)	(((addr) & 0x1fffff) | ((slot) << 21))
#define USSE_LOAD_HI(addr)		(((((addr) >> 21) << 4) & 0x1f0) | ((((addr) >> 26) << 12) & 0x3f000) | 0xfca00000)

/* the sequence's last instruction */
#define USSE_LOAD_END_LO(count)		((((count) << 22) & 0x0fc00000) | 0x200082)
#define USSE_LOAD_END_HI(count)		((((count) << 10) & 0x00c00000) | (((count) >> 3) & 0x1f8) | 0xfb240000)

SceUInt32 *SceGpuEs4User_86FC3202(SceUInt32 *out)
{
	out[0] = USSE_LOAD_HEADER_LO;
	out[1] = USSE_LOAD_HEADER_HI;
	out[2] = USSE_LOAD_LO(0, 0);
	out[3] = USSE_LOAD_HI(0);
	out[4] = USSE_LOAD_LO(1, 0);
	out[5] = USSE_LOAD_HI(0);
	out[6] = 0x81200000;		/* encoding unknown */
	out[7] = 0xfb260000;
	return out + 8;
}

SceUInt32 *SceGpuEs4User_53CACDAB(SceUInt32 *out, const SceUInt32 *address, SceUInt32 count)
{
	SceUInt32 *insn = out + 2;
	int i;

	out[0] = USSE_LOAD_HEADER_LO;
	out[1] = USSE_LOAD_HEADER_HI;
	for (i = 0; i < USSE_LOAD_SLOTS; i++) {
		SceUInt32 addr = address[i];

		insn[2 * i] = USSE_LOAD_LO(i, addr);
		insn[2 * i + 1] = USSE_LOAD_HI(addr);
	}
	out[14] = USSE_LOAD_END_LO(count);
	out[15] = USSE_LOAD_END_HI(count);
	return out + 16;
}
