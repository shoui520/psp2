/*
 * SceGpuEs4User: PDS (programmable data sequencer) program generators.
 *
 * SceGpuEs4User_E98914E4 copies one of five fixed PDS programs and patches
 * its constants from the descriptor; SceGpuEs4User_A5E331FA writes a state
 * update program. Both leave the 16-byte aligned program address and size in
 * the descriptor and return the end of the buffer used.
 */

#include <libkernel/libkernel.h>
#include "gpu_es4_user.h"

#define PDS_PROGRAM_COPY_SIZE	0x90	/* copied whatever the program's length */
#define PDS_PROGRAM_SIZE_SHORT	0x30
#define PDS_PROGRAM_SIZE_LONG	0x40

/*
 * PLACEHOLDER: SGX PDS microcode. The five program templates are still the
 * original instruction words; they are not yet reverse engineered into PDS
 * source and assembled.
 *
 * Programs 0, 2 and 4 are 0x68 bytes, 1 and 3 0x90 bytes, back to back:
 * copying PDS_PROGRAM_COPY_SIZE bytes of a short program reads into the next
 * one, as Sony's does. After program 4 Sony's copy reads the module's unwind
 * table (layout dependent); here it reads the zero padding.
 */
#define PDS_PROGRAM_0	0
#define PDS_PROGRAM_1	26
#define PDS_PROGRAM_2	62
#define PDS_PROGRAM_3	88
#define PDS_PROGRAM_4	124

static const SceUInt32 s_pdsPrograms[] = {
	/* program 0: not textured, not perspective */
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0xfffeffff, 0x00000100, 0x00000000, 0x00020000, 0x00000000,
	0x93800003, 0x07020185, 0xaf000000, 0x9480000b, 0xcf088230, 0x87600000,
	0xcf800630, 0xc7600830, 0x10000a60, 0x07600b05, 0xaf000000, 0x07041185,
	0xaf000000, 0x00000000,
	/* program 1: textured, variant 2 */
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x000000ff, 0xffff0000, 0x00000100, 0x0000ff00, 0xfffeffff,
	0x00000000, 0x00000000, 0x00020000, 0x00000000, 0x9380000a, 0xcf088070,
	0xcf800630, 0xf7300030, 0xc7606070, 0xcf800830, 0xf7300130, 0xc7606070,
	0x07018185, 0xaf000000, 0x94800012, 0xcf0a8230, 0x87600000, 0xcf800a30,
	0xc7600c30, 0x10000c60, 0x07600b05, 0xaf000000, 0x07041185, 0xaf000000,
	/* program 2: not textured, perspective, variant 2 */
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0xfffeffff, 0x00000100, 0x00000000, 0x00020000, 0x00000000,
	0x93800003, 0x07020185, 0xaf000000, 0x9480000b, 0xcf088230, 0x87600000,
	0xcf800630, 0xc7600830, 0x10000a60, 0x07600b05, 0xaf000000, 0x07041185,
	0xaf000000, 0x00000000,
	/* program 3: textured, other variants */
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x000000ff, 0xffff0000, 0x00000100, 0x0000ff00, 0xfffeffff,
	0x00000000, 0x00000000, 0x00020000, 0x00000000, 0x9380000a, 0xcf088070,
	0xcf800630, 0xf7300130, 0xc7606070, 0xcf800830, 0xf7300130, 0xc7606070,
	0x07018185, 0xaf000000, 0x94800012, 0xcf0a8230, 0x87600000, 0xcf800a30,
	0xc7600c30, 0x10000c60, 0x07600b05, 0xaf000000, 0x07041185, 0xaf000000,
	/* program 4: not textured, perspective, other variants */
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0xfffeffff, 0x00000100, 0x00000000, 0x00020000, 0x00000000,
	0x93800003, 0x07020185, 0xaf000000, 0x9480000b, 0xcf088230, 0x87600000,
	0xcf800630, 0xc7600830, 0x10000a60, 0x07600b05, 0xaf000000, 0x07041185,
	0xaf000000, 0x00000000,
	/* read past program 4 by the fixed-size copy */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

_Static_assert(sizeof(s_pdsPrograms) == PDS_PROGRAM_4 * 4 + PDS_PROGRAM_COPY_SIZE, "program table");

static inline SceUInt32 Align16(SceUInt32 address)
{
	return (address + 15) & ~15;
}

SceUInt32 *SceGpuEs4User_E98914E4(GpuPdsProgram *pds, SceUInt32 *buffer, int textured, int perspective,
				  int variant)
{
	if (textured != 0) {
		sceClibMemcpy_safe(buffer, &s_pdsPrograms[variant == 2 ? PDS_PROGRAM_1 : PDS_PROGRAM_3],
			      PDS_PROGRAM_COPY_SIZE);
		pds->size = PDS_PROGRAM_SIZE_LONG;
		buffer[0] = pds->constant[0];
		buffer[1] = pds->constant[1];
		buffer[4] = pds->constant[3];
		buffer[5] = pds->constant[4];
		buffer[6] = pds->constant[5];
		buffer[2] = pds->constant[6];
		buffer[3] = pds->constant[7];
		buffer[12] = pds->constant[8];
	} else {
		const SceUInt32 *program;

		if (perspective == 0)
			program = &s_pdsPrograms[PDS_PROGRAM_0];
		else if (variant == 2)
			program = &s_pdsPrograms[PDS_PROGRAM_2];
		else
			program = &s_pdsPrograms[PDS_PROGRAM_4];
		sceClibMemcpy_safe(buffer, program, PDS_PROGRAM_COPY_SIZE);
		pds->size = PDS_PROGRAM_SIZE_SHORT;
		buffer[0] = pds->constant[0];
		buffer[1] = pds->constant[1];
		buffer[4] = pds->constant[3];
		buffer[5] = pds->constant[4];
		buffer[6] = pds->constant[5];
		buffer[2] = pds->constant[6];
		buffer[3] = pds->constant[7];
		buffer[9] = pds->constant[8];
	}
	pds->program = Align16((SceUInt32)buffer);
	return buffer + PDS_PROGRAM_COPY_SIZE / sizeof(SceUInt32);
}

/* PLACEHOLDER: PDS instruction words of the state update program, not decoded */
#define PDS_STATE_UPDATE_INSN0	0x070181a6
#define PDS_STATE_UPDATE_INSN1	0x070581a6
#define PDS_STATE_UPDATE_INSN2	0x070801a5
#define PDS_STATE_UPDATE_INSN3	0xaf000000

SceUInt32 *SceGpuEs4User_A5E331FA(GpuPdsProgram *pds, void *buffer)
{
	SceUInt32 *program = (SceUInt32 *)Align16((SceUInt32)buffer);

	program[0] = 0x2000;
	program[1] = 0;
	program[5] = 0x100;
	program[4] = pds->constant[3];
	program[8] = pds->constant[0];
	program[9] = pds->constant[1];
	program[2] = pds->constant[2];
	program[12] = PDS_STATE_UPDATE_INSN0;
	program[13] = PDS_STATE_UPDATE_INSN1;
	program[14] = PDS_STATE_UPDATE_INSN1;
	program[15] = PDS_STATE_UPDATE_INSN1;
	program[16] = PDS_STATE_UPDATE_INSN2;
	program[17] = PDS_STATE_UPDATE_INSN3;
	pds->program = (SceUInt32)program;
	pds->size = PDS_PROGRAM_SIZE_SHORT;
	return program + 18;
}

/* USSE end of program (PLACEHOLDER: instruction encoding not decoded) */
#define USSE_INSN_END_LO	0x00000000
#define USSE_INSN_END_HI	0xfa4c0700

SceUInt32 *SceGpuEs4User_1A575DFD(SceUInt32 *out)
{
	out[0] = USSE_INSN_END_LO;
	out[1] = USSE_INSN_END_HI;
	return out + 2;
}
