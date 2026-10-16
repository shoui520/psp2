#ifndef SCE_BACKTRACE_UNWIND_H
#define SCE_BACKTRACE_UNWIND_H

#include <stdint.h>

/* ARM EHABI unwind opcodes (section 10.3), by their leading bits */
#define EHABI_VSP_ADD			0x00	/* 00xxxxxx: vsp += (x << 2) + 4 */
#define EHABI_VSP_SUB			0x40	/* 01xxxxxx: vsp -= (x << 2) + 4 */
#define EHABI_VSP_MASK			0xc0
#define EHABI_POP_MASK			0x80	/* 1000iiii iiiiiiii: pop {r4-r15} by mask */
#define EHABI_SET_VSP			0x90	/* 1001nnnn: vsp = r[n] */
#define EHABI_POP_R4			0xa0	/* 1010Lnnn: pop {r4-r[4+n]}, L: and r14 */
#define EHABI_POP_R4_LR			0x08
#define EHABI_GROUP_MASK		0xf0
#define EHABI_FINISH			0xb0
#define EHABI_POP_R0_R3			0xb1	/* 10110001 0000iiii: pop {r0-r3} by mask */
#define EHABI_VSP_ADD_ULEB128		0xb2	/* vsp += 0x204 + (uleb128 << 2) */
#define EHABI_POP_VFP_FSTMFDX		0xb3	/* 10110011 sssscccc */
#define EHABI_POP_VFP_D8_FSTMFDX	0xb8	/* 10111nnn: pop {d8-d[8+n]} */
#define EHABI_POP_VFP_D16		0xc8	/* 11001000 sssscccc: pop {d[16+s]-...} */
#define EHABI_POP_VFP			0xc9	/* 11001001 sssscccc */
#define EHABI_SHORT_MASK		0xf8

enum BacktraceRegister {
	BACKTRACE_SP = 13,
	BACKTRACE_LR = 14,
	BACKTRACE_PC = 15,
};

static inline __attribute__((always_inline))
unsigned int BacktraceInstructionByte(uint32_t word, unsigned int index)
{
	return (word >> (index * 8)) & 0xff;
}

static inline __attribute__((always_inline))
uint32_t BacktraceSmallStackAdjustment(unsigned int instruction)
{
	return (instruction & 0x3f) * 4 + 4;
}

/* These opcodes discard saved floating-point registers during a backtrace. */
static inline __attribute__((always_inline))
uint32_t BacktraceFloatingStackAdjustment(unsigned int instruction,
		unsigned int operand)
{
	if ((instruction & EHABI_SHORT_MASK) == EHABI_POP_VFP_D8_FSTMFDX)
		return (instruction & 7) * 8 + 12;
	if (instruction == EHABI_POP_VFP_FSTMFDX)
		return (operand & 0xf) * 8 + 12;
	return ((operand & 0xf) + 1) * 8;
}

#endif
