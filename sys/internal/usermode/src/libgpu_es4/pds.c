/*
 * SceGpuEs4User: PDS (programmable data sequencer) program generators.
 *
 * SceGpuEs4User_E98914E4 copies one of five fixed PDS programs and patches
 * its constants from the descriptor; SceGpuEs4User_A5E331FA writes a state
 * update program. Both leave the 16-byte aligned program address and size in
 * the descriptor and return the end of the buffer used.
 *
 * The programs are Imagination's pixel event programs, written in pds/ and
 * assembled by the SGX543's own assembler (pdsasm, CMakeLists.txt), which also
 * writes their data segments: the task control blocks the caller patches. The
 * five templates are three programs, the untextured one and the 2x and 4x
 * multisampled ones, which build their tile coordinates differently (BRN31175).
 */

#include <libkernel/libkernel.h>
#include "gpu_es4_user.h"
#include "end_code.h"
#include "stateupdate_code.h"

#define PDS_PROGRAM_COPY_SIZE	0x90	/* copied whatever the program's length */
#define PDS_PROGRAM_SIZE_SHORT	0x30
#define PDS_PROGRAM_SIZE_LONG	0x40

/*
 * Programs 0, 2 and 4 are 104 bytes plus one zero word (PDS addresses programs
 * in instruction pairs), 1 and 3 144 bytes, back to back: copying
 * PDS_PROGRAM_COPY_SIZE bytes of a short program reads into the next one, as
 * Sony's does. After program 4 Sony's copy reads the module's unwind table
 * (layout dependent); here it reads the zero padding.
 */
#define PDS_PROGRAM_0	0
#define PDS_PROGRAM_1	26
#define PDS_PROGRAM_2	62
#define PDS_PROGRAM_3	88
#define PDS_PROGRAM_4	124

static const SceUInt32 s_pdsPrograms[] = {
	/* program 0: not textured, not perspective */
#include "pixelevent.inc"
	0x00000000,				/* program alignment */
	/* program 1: textured, 2x multisampling */
#include "pixelevent_msnods_2x.inc"
	/* program 2: not textured, perspective, variant 2 */
#include "pixelevent.inc"
	0x00000000,
	/* program 3: textured, 4x multisampling */
#include "pixelevent_msnods_4x.inc"
	/* program 4: not textured, perspective, other variants */
#include "pixelevent.inc"
	0x00000000,
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

/*
 * The state update program (pds/stateupdate.pds.asm): the state write and the
 * tile rectangle as attribute words, three times, then the USE task word.
 * Sony's compiler keeps its instructions in this function as immediate stores,
 * so they are here as the assembler's words rather than a table.
 */
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
	program[12] = PDS_STATEUPDATE_CODE_INSN0;
	program[13] = PDS_STATEUPDATE_CODE_INSN1;
	program[14] = PDS_STATEUPDATE_CODE_INSN2;
	program[15] = PDS_STATEUPDATE_CODE_INSN3;
	program[16] = PDS_STATEUPDATE_CODE_INSN4;
	program[17] = PDS_STATEUPDATE_CODE_INSN5;
	pds->program = (SceUInt32)program;
	pds->size = PDS_PROGRAM_SIZE_SHORT;
	return program + 18;
}

/* USSE end of program (ukernel/end.use.asm: phas.end #0, #0, pixel, end, parallel) */
SceUInt32 *SceGpuEs4User_1A575DFD(SceUInt32 *out)
{
	out[0] = PDS_END_CODE_INSN0;
	out[1] = PDS_END_CODE_INSN1;
	return out + 2;
}
