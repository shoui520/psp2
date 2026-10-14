/*
 * SceGxm: encoding of the float16 blend program list.
 */

#include "blending_private.h"

/* how an operand is encoded in one instruction slot */
typedef struct GxmBlendOperand {
	SceInt32 bank;
	SceUInt32 extended;		/* extended bank bit */
	SceUInt32 number;		/* register number */
} GxmBlendOperand;

typedef struct GxmBlendRegister {
	const char *name;
	GxmBlendOperand dst;
	GxmBlendOperand src0;
	GxmBlendOperand src1;
	GxmBlendOperand src2;
} GxmBlendRegister;

static const GxmBlendRegister gxmBlendRegisters[] = {
	[GXM_BLEND_REG_NONE] = { "<none>", { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 } },
	[GXM_BLEND_REG_O0] = { "o0", { 1, 0, 0 }, { 0, 1, 0 }, { 1, 0, 0 }, { 1, 0, 0 } },
	[GXM_BLEND_REG_P0] = { "p0", { 2, 0, 0 }, { 1, 0, 0 }, { 2, 0, 0 }, { 2, 0, 0 } },
	[GXM_BLEND_REG_GPI0] = { "gpi0", { 0, 0, 60 }, { 0, 0, 60 }, { 0, 0, 60 }, { 0, 0, 60 } },
	[GXM_BLEND_REG_GPI1] = { "gpi1", { 0, 0, 61 }, { 0, 0, 61 }, { 0, 0, 61 }, { 0, 0, 61 } },
	[GXM_BLEND_REG_GPI2] = { "gpi2", { 0, 0, 62 }, { 0, 0, 62 }, { 0, 0, 62 }, { 0, 0, 62 } },
	[GXM_BLEND_REG_R0] = { "r0", { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
	[GXM_BLEND_REG_R1] = { "r1", { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } },
	[GXM_BLEND_REG_ZERO] = { "0.f", { -1, 0, 0 }, { -1, 0, 0 }, { 1, 1, 33 }, { 1, 1, 33 } },
	[GXM_BLEND_REG_ONE] = { "1.f", { -1, 0, 0 }, { -1, 0, 0 }, { 1, 1, 34 }, { 1, 1, 34 } },
};

/* the write mask field, components in the hardware's order */
static const SceUInt32 gxmBlendMaskBits[16] = {
	0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15,
};

/* the operation field of the two-source instruction */
static const SceUInt32 gxmBlendOpcodes[] = {
	[GXM_BLEND_OP_NOP] = 0,
	[GXM_BLEND_OP_MAD] = 0,
	[GXM_BLEND_OP_MUL] = 0,
	[GXM_BLEND_OP_MIN] = 5,
	[GXM_BLEND_OP_MAX] = 6,
	[GXM_BLEND_OP_ADD] = 1,
};

/* the output register is p0 plus half the register index */
static inline SceUInt32 gxmBlendRegNumber(SceUInt32 index, const GxmBlendOperand *operand,
					  SceUInt32 reg)
{
	if (index == GXM_BLEND_REG_P0)
		return operand->number + (reg >> 1);
	return operand->number;
}

static SceUInt32 *gxmBlendAssembleNop(const GxmBlendInstr *instr, SceUInt32 *code, SceUInt32 reg)
{
	code[0] = 0;
	code[1] = 0xf8000140;
	return code + 2;
}

static SceUInt32 *gxmBlendAssembleMad(const GxmBlendInstr *instr, SceUInt32 *code, SceUInt32 reg)
{
	const GxmBlendRegister *regs = gxmBlendRegisters;
	SceUInt32 dstNumber, src0Number, src1Number, src2Number;

	dstNumber = gxmBlendRegNumber(instr->dst, &regs[instr->dst].dst, reg);
	src0Number = gxmBlendRegNumber(instr->src0, &regs[instr->src0].src0, reg);
	src1Number = gxmBlendRegNumber(instr->src1, &regs[instr->src1].src1, reg);
	src2Number = gxmBlendRegNumber(instr->src2, &regs[instr->src2].src2, reg);

	code[0] = dstNumber << 22 | src0Number << 12 | src1Number << 6 | src2Number;
	code[0] |= regs[instr->src1].src1.bank << 30;
	code[0] |= regs[instr->src2].src2.bank << 28;
	code[1] = regs[instr->dst].dst.bank | regs[instr->src0].src0.bank << 2 |
		  gxmBlendMaskBits[instr->mask] << 7 |
		  (regs[instr->dst].dst.extended ? 0x80000 : 0) |
		  (regs[instr->src1].src1.extended ? 0x20000 : 0) |
		  (regs[instr->src2].src2.extended ? 0x10000 : 0) |
		  (instr->negate ? 0x20 : 0) | 0x04800000;
	if (!instr->src0Alpha)
		code[1] |= 0x00200000;
	else
		code[0] |= 0x000c0000;
	if (!instr->src1Alpha)
		code[1] |= 0x00001000;
	else
		code[0] |= 0x00300000;
	if (!instr->src2Alpha)
		code[1] |= 0x00008000;
	else
		code[1] |= 0x00006000;
	return code + 2;
}

static SceUInt32 *gxmBlendAssembleOp2(const GxmBlendInstr *instr, SceUInt32 *code,
				      SceUInt32 reg)
{
	const GxmBlendRegister *regs = gxmBlendRegisters;
	SceUInt32 dstNumber, src1Number, src2Number;

	dstNumber = gxmBlendRegNumber(instr->dst, &regs[instr->dst].dst, reg);
	src1Number = gxmBlendRegNumber(instr->src1, &regs[instr->src1].src1, reg);
	src2Number = gxmBlendRegNumber(instr->src2, &regs[instr->src2].src2, reg);

	code[1] = 0x10800000;
	code[0] = regs[instr->src1].src1.bank << 30 | regs[instr->src2].src2.bank << 28 |
		  gxmBlendOpcodes[instr->op] << 12 |
		  dstNumber << 22 | src1Number << 6 | src2Number;
	code[1] |= gxmBlendMaskBits[instr->mask] << 7;
	code[1] |= regs[instr->dst].dst.extended ? 0x80000 : 0;
	code[1] |= regs[instr->dst].dst.bank;
	code[1] |= regs[instr->src1].src1.extended ? 0x20000 : 0;
	code[1] |= regs[instr->src2].src2.extended ? 0x10000 : 0;
	code[1] |= 0x00240004;
	code[0] |= instr->src1Alpha ? 0x002d8000 : 0x00040000;
	code[1] |= instr->src2Alpha ? 0x00003000 : 0x00004000;
	if (instr->negate)
		code[1] |= 0x20;
	return code + 2;
}

static SceUInt32 *(*const gxmBlendAssemblers[])(const GxmBlendInstr *instr, SceUInt32 *code,
						SceUInt32 reg) = {
	[GXM_BLEND_OP_NOP] = gxmBlendAssembleNop,
	[GXM_BLEND_OP_MAD] = gxmBlendAssembleMad,
	[GXM_BLEND_OP_MUL] = gxmBlendAssembleOp2,
	[GXM_BLEND_OP_MIN] = gxmBlendAssembleOp2,
	[GXM_BLEND_OP_MAX] = gxmBlendAssembleOp2,
	[GXM_BLEND_OP_ADD] = gxmBlendAssembleOp2,
};

/* encode one operation as one instruction */
SceUInt32 *gxmBlendAssemble(const GxmBlendInstr *instr, SceUInt32 *code, SceUInt32 reg)
{
	return gxmBlendAssemblers[instr->op](instr, code, reg);
}
