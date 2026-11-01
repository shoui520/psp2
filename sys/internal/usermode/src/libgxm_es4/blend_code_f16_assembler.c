/*
 * SceGxm: encoding of the float16 blend program list.
 */

#include "blending_private.h"

/* how an operand is encoded in one instruction slot */
typedef struct GxmBlendOperand {
	SceInt32 bank;
	SceUInt32 extendedBank;
	SceUInt32 number;		/* register number */
} GxmBlendOperand;

typedef struct GxmBlendRegister {
	const char *name;
	GxmBlendOperand dst;
	GxmBlendOperand s0;
	GxmBlendOperand s1;
	GxmBlendOperand s2;
} GxmBlendRegister;

static const GxmBlendRegister s_registerDescriptions[] = {
	[REG_NONE] = { "<none>", { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 } },
	[REG_O0] = { "o0", { 1, 0, 0 }, { 0, 1, 0 }, { 1, 0, 0 }, { 1, 0, 0 } },
	[REG_P0] = { "p0", { 2, 0, 0 }, { 1, 0, 0 }, { 2, 0, 0 }, { 2, 0, 0 } },
	[REG_GPI0] = { "gpi0", { 0, 0, 60 }, { 0, 0, 60 }, { 0, 0, 60 }, { 0, 0, 60 } },
	[REG_GPI1] = { "gpi1", { 0, 0, 61 }, { 0, 0, 61 }, { 0, 0, 61 }, { 0, 0, 61 } },
	[REG_GPI2] = { "gpi2", { 0, 0, 62 }, { 0, 0, 62 }, { 0, 0, 62 }, { 0, 0, 62 } },
	[REG_R0] = { "r0", { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
	[REG_R1] = { "r1", { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } },
	[REG_ZERO] = { "0.f", { -1, 0, 0 }, { -1, 0, 0 }, { 1, 1, 33 }, { 1, 1, 33 } },
	[REG_ONE] = { "1.f", { -1, 0, 0 }, { -1, 0, 0 }, { 1, 1, 34 }, { 1, 1, 34 } },
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
	if (index == REG_P0)
		return operand->number + (reg >> 1);
	return operand->number;
}

static SceUInt32 *gxmBlendAssembleNop(const GxmBlendInstr *op, SceUInt32 *code, SceUInt32 reg)
{
	code[0] = 0;
	code[1] = 0xf8000140;
	return code + 2;
}

static SceUInt32 *gxmBlendAssembleMad(const GxmBlendInstr *op, SceUInt32 *code, SceUInt32 reg)
{
	const GxmBlendRegister *regs = s_registerDescriptions;
	SceUInt32 dstNumber, src0Number, src1Number, src2Number;

	dstNumber = gxmBlendRegNumber(op->dst, &regs[op->dst].dst, reg);
	src0Number = gxmBlendRegNumber(op->src0, &regs[op->src0].s0, reg);
	src1Number = gxmBlendRegNumber(op->src1, &regs[op->src1].s1, reg);
	src2Number = gxmBlendRegNumber(op->src2, &regs[op->src2].s2, reg);
	GXM_DBG_ASSERT(167, !s_registerDescriptions[op->src0].s0.extendedBank);
	GXM_DBG_ASSERT(168, (REG_GPI0 == op->dst) || (REG_GPI1 == op->dst) ||
			    (REG_GPI2 == op->dst) || (15 == op->wmask) || (3 == op->wmask) ||
			    (12 == op->wmask));

	code[0] = dstNumber << 22 | src0Number << 12 | src1Number << 6 | src2Number;
	code[0] |= regs[op->src1].s1.bank << 30;
	code[0] |= regs[op->src2].s2.bank << 28;
	code[1] = regs[op->dst].dst.bank | regs[op->src0].s0.bank << 2 |
		  gxmBlendMaskBits[op->wmask] << 7 |
		  (regs[op->dst].dst.extendedBank ? 0x80000 : 0) |
		  (regs[op->src1].s1.extendedBank ? 0x20000 : 0) |
		  (regs[op->src2].s2.extendedBank ? 0x10000 : 0) |
		  (op->negate ? 0x20 : 0) | 0x04800000;
	if (!op->src0Alpha)
		code[1] |= 0x00200000;
	else
		code[0] |= 0x000c0000;
	if (!op->src1Alpha)
		code[1] |= 0x00001000;
	else
		code[0] |= 0x00300000;
	if (!op->src2Alpha)
		code[1] |= 0x00008000;
	else
		code[1] |= 0x00006000;
	return code + 2;
}

static SceUInt32 *gxmBlendAssembleOp2(const GxmBlendInstr *op, SceUInt32 *code,
				      SceUInt32 reg)
{
	const GxmBlendRegister *regs = s_registerDescriptions;
	SceUInt32 dstNumber, src1Number, src2Number;

	dstNumber = gxmBlendRegNumber(op->dst, &regs[op->dst].dst, reg);
	src1Number = gxmBlendRegNumber(op->src1, &regs[op->src1].s1, reg);
	src2Number = gxmBlendRegNumber(op->src2, &regs[op->src2].s2, reg);

	code[1] = 0x10800000;
	code[0] = regs[op->src1].s1.bank << 30 | regs[op->src2].s2.bank << 28 |
		  gxmBlendOpcodes[op->op] << 12 |
		  dstNumber << 22 | src1Number << 6 | src2Number;
	code[1] |= gxmBlendMaskBits[op->wmask] << 7;
	code[1] |= regs[op->dst].dst.extendedBank ? 0x80000 : 0;
	code[1] |= regs[op->dst].dst.bank;
	code[1] |= regs[op->src1].s1.extendedBank ? 0x20000 : 0;
	code[1] |= regs[op->src2].s2.extendedBank ? 0x10000 : 0;
	code[1] |= 0x00240004;
	code[0] |= op->src1Alpha ? 0x002d8000 : 0x00040000;
	code[1] |= op->src2Alpha ? 0x00003000 : 0x00004000;
	if (op->negate)
		code[1] |= 0x20;
	return code + 2;
}

static SceUInt32 *(*const gxmBlendAssemblers[])(const GxmBlendInstr *op, SceUInt32 *code,
						SceUInt32 reg) = {
	[GXM_BLEND_OP_NOP] = gxmBlendAssembleNop,
	[GXM_BLEND_OP_MAD] = gxmBlendAssembleMad,
	[GXM_BLEND_OP_MUL] = gxmBlendAssembleOp2,
	[GXM_BLEND_OP_MIN] = gxmBlendAssembleOp2,
	[GXM_BLEND_OP_MAX] = gxmBlendAssembleOp2,
	[GXM_BLEND_OP_ADD] = gxmBlendAssembleOp2,
};

/* encode one operation as one instruction */
SceUInt32 *gxmBlendAssemble(const GxmBlendInstr *op, SceUInt32 *code, SceUInt32 reg)
{
	return gxmBlendAssemblers[op->op](op, code, reg);
}
