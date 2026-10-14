/*
 * SceGxm: optimiser for the float16 blend program list.
 *
 * Three passes over the list (the last entry is the terminating NOP):
 *  1. an operation that only copies a value or a constant is folded into
 *     the operations that read its result and removed;
 *  2. a multiply whose result only feeds adds is folded into them as a
 *     multiply-add and removed;
 *  3. r1 is renamed r0 when r0 ends up unused.
 * The component state of the register being followed is tracked per
 * component (A, R, G, B, as in the colour mask).
 */

#include "blending_private.h"

#define GXM_BLEND_UNWRITTEN	0
#define GXM_BLEND_PENDING	1	/* holds the followed operation's result */
#define GXM_BLEND_OVERWRITTEN	2
#define GXM_BLEND_READ		4	/* read before the followed operation */

static inline void gxmBlendStateInit(SceUInt32 *state, SceUInt32 mask)
{
	state[0] = mask & 1;
	state[1] = (mask >> 1) & 1;
	state[2] = (mask >> 2) & 1;
	state[3] = (mask >> 3) & 1;
}

static inline void gxmBlendStateSet(SceUInt32 *state, SceUInt32 mask, SceUInt32 value)
{
	if (mask & 1)
		state[0] = value;
	if (mask & 2)
		state[1] = value;
	if (mask & 4)
		state[2] = value;
	if (mask & 8)
		state[3] = value;
}

/* every component in mask still holds the result */
static inline SceBool gxmBlendCovers(const SceUInt32 *state, SceUInt32 mask)
{
	return (!(mask & 1) || state[0] == GXM_BLEND_PENDING) &&
	       (!(mask & 2) || state[1] == GXM_BLEND_PENDING) &&
	       (!(mask & 4) || state[2] == GXM_BLEND_PENDING) &&
	       (!(mask & 8) || state[3] == GXM_BLEND_PENDING);
}

/* some component in mask still holds the result */
static inline SceBool gxmBlendTouches(const SceUInt32 *state, SceUInt32 mask)
{
	return ((mask & 1) && state[0] == GXM_BLEND_PENDING) ||
	       ((mask & 2) && state[1] == GXM_BLEND_PENDING) ||
	       ((mask & 4) && state[2] == GXM_BLEND_PENDING) ||
	       ((mask & 8) && state[3] == GXM_BLEND_PENDING);
}

/* the components an operand reads */
static inline SceUInt32 gxmBlendReadMask(const GxmBlendInstr *instr, SceUInt8 alpha)
{
	return alpha ? SCE_GXM_COLOR_MASK_A : instr->mask;
}

/* the operand reg is read entirely from the followed result */
static inline SceBool gxmBlendReadsAll(const GxmBlendInstr *instr, SceUInt32 operand,
				       SceUInt8 alpha, SceUInt32 reg, const SceUInt32 *state)
{
	return operand == reg && gxmBlendCovers(state, gxmBlendReadMask(instr, alpha));
}

/* the operand reg is read partly from the followed result */
static inline SceBool gxmBlendReadsAny(const GxmBlendInstr *instr, SceUInt32 operand,
				       SceUInt8 alpha, SceUInt32 reg, const SceUInt32 *state)
{
	return operand == reg && gxmBlendTouches(state, gxmBlendReadMask(instr, alpha));
}

/* writing instr's register changes the operand */
static inline SceBool gxmBlendClobbers(const GxmBlendInstr *instr, const GxmBlendInstr *reader,
				       SceUInt32 operand, SceUInt8 alpha)
{
	return instr->dst == operand &&
	       ((alpha && (instr->mask & SCE_GXM_COLOR_MASK_A)) || (instr->mask & reader->mask));
}

static inline void gxmBlendRemove(GxmBlendInstr *instr, GxmBlendInstr *end)
{
	GxmBlendInstr *p;

	for (p = instr + 1; p < end; p++)
		p[-1] = *p;
}

/* the operation copies a value: which one */
static inline SceBool gxmBlendIsCopy(const GxmBlendInstr *instr, SceUInt32 *value)
{
	switch (instr->op) {
	case GXM_BLEND_OP_NOP:
	case GXM_BLEND_OP_MAD:
		if (instr->src1 != GXM_BLEND_REG_ZERO)
			return SCE_FALSE;
		*value = instr->op == GXM_BLEND_OP_MAD ? instr->src2 : GXM_BLEND_REG_NONE;
		return SCE_TRUE;
	case GXM_BLEND_OP_MUL:
		if (instr->src2 == GXM_BLEND_REG_ZERO || instr->src1 == GXM_BLEND_REG_ZERO) {
			*value = GXM_BLEND_REG_ZERO;
			return SCE_TRUE;
		}
		if (instr->src2 == GXM_BLEND_REG_ONE || instr->src1 == GXM_BLEND_REG_ONE) {
			*value = instr->src1 != GXM_BLEND_REG_ONE ? instr->src1 : instr->src2;
			return SCE_TRUE;
		}
		return SCE_FALSE;
	case GXM_BLEND_OP_ADD:
		if (instr->src2 == GXM_BLEND_REG_ZERO || instr->src1 == GXM_BLEND_REG_ZERO) {
			*value = instr->src1 != GXM_BLEND_REG_ZERO ? instr->src1 : instr->src2;
			return SCE_TRUE;
		}
		return SCE_FALSE;
	}
	return SCE_FALSE;
}

/* user reads reg, which holds value: read value instead */
static inline void gxmBlendSubstitute(GxmBlendInstr *user, SceUInt32 reg, SceUInt32 value,
				      SceBool *kept)
{
	switch (user->op) {
	case GXM_BLEND_OP_MAD:
		if (value == GXM_BLEND_REG_ZERO) {
			if (reg == user->src0 || reg == user->src1) {
				/* 0 * x + src2 */
				user->op = GXM_BLEND_OP_ADD;
				user->src0 = GXM_BLEND_REG_NONE;
				user->src1 = GXM_BLEND_REG_ZERO;
				user->negate = 0;
				user->src0Alpha = 0;
				user->src1Alpha = 0;
			} else {
				/* src0 * src1 + 0 */
				user->op = GXM_BLEND_OP_MUL;
				user->src2 = user->src0;
				user->src2Alpha = user->src0Alpha;
				user->src0 = GXM_BLEND_REG_NONE;
				user->src0Alpha = 0;
			}
		} else if (value == GXM_BLEND_REG_ONE) {
			if (reg == user->src0) {
				/* 1 * src1 + src2 */
				user->op = GXM_BLEND_OP_ADD;
				user->src0 = GXM_BLEND_REG_NONE;
				user->src0Alpha = 0;
			} else if (reg == user->src1) {
				/* src0 * 1 + src2 */
				user->op = GXM_BLEND_OP_ADD;
				user->src1 = user->src0;
				user->src1Alpha = user->src0Alpha;
				user->src0 = GXM_BLEND_REG_NONE;
				user->src0Alpha = 0;
			} else {
				user->src2 = GXM_BLEND_REG_ONE;
			}
		} else {
			*kept = SCE_TRUE;
		}
		break;
	case GXM_BLEND_OP_MUL:
	case GXM_BLEND_OP_MIN:
	case GXM_BLEND_OP_MAX:
	case GXM_BLEND_OP_ADD:
		if (user->src1 == reg)
			user->src1 = value;
		if (user->src2 == reg)
			user->src2 = value;
		if (user->op == GXM_BLEND_OP_MUL && reg == GXM_BLEND_REG_ZERO) {
			user->op = GXM_BLEND_OP_ADD;
			user->src1 = GXM_BLEND_REG_ZERO;
			user->src2 = GXM_BLEND_REG_ZERO;
		}
		break;
	}
}

/* store src0 * src1 + src2, simplified for a zero or one factor */
static inline void gxmBlendSetMad(GxmBlendInstr *instr, SceUInt32 op,
				  SceUInt32 src0, SceUInt8 src0Alpha,
				  SceUInt32 src1, SceUInt8 src1Alpha,
				  SceUInt32 src2, SceUInt8 src2Alpha, SceUInt8 negate)
{
	if (src0 == GXM_BLEND_REG_ZERO || src1 == GXM_BLEND_REG_ZERO) {
		op = GXM_BLEND_OP_ADD;
		src0 = GXM_BLEND_REG_NONE;
		src0Alpha = 0;
		src1 = GXM_BLEND_REG_ZERO;
		negate = 0;
	} else if (src1 == GXM_BLEND_REG_ONE || src0 == GXM_BLEND_REG_ONE) {
		if (src1 == GXM_BLEND_REG_ONE) {
			src1 = src0;
			src1Alpha = src0Alpha;
		}
		op = GXM_BLEND_OP_MUL;
		src0 = GXM_BLEND_REG_NONE;
		src0Alpha = 0;
	}
	instr->op = op;
	instr->src0 = src0;
	instr->src0Alpha = src0Alpha;
	instr->src2 = src2;
	instr->src1 = src1;
	instr->negate = negate;
	instr->src1Alpha = src1Alpha;
	instr->src2Alpha = src2Alpha;
}

/*
 * Fold the multiply mul into the add user, which reads its result as
 * src1 (viaSrc1) or src2.  Fails when both factors are o0: the
 * multiply-add cannot take o0 as its first factor.
 */
static inline SceBool gxmBlendFoldMultiply(GxmBlendInstr *user, const GxmBlendInstr *mul,
					   SceBool viaSrc1)
{
	SceUInt32 other;
	SceUInt8 otherAlpha, negate, alpha1, alpha2;

	if (viaSrc1) {
		other = user->src2;
		otherAlpha = user->src2Alpha;
	} else {
		other = user->src1;
		otherAlpha = user->src1Alpha;
	}
	negate = user->negate ^ mul->negate;
	alpha1 = user->src1Alpha | mul->src1Alpha;
	alpha2 = user->src2Alpha | mul->src2Alpha;

	if (other == GXM_BLEND_REG_ZERO)
		gxmBlendSetMad(user, GXM_BLEND_OP_MUL, GXM_BLEND_REG_NONE, alpha1,
			       mul->src2, alpha2, mul->src1, alpha1, negate);
	else if (mul->src1 != GXM_BLEND_REG_O0)
		gxmBlendSetMad(user, GXM_BLEND_OP_MAD, mul->src1, alpha1,
			       mul->src2, alpha2, other, otherAlpha, negate);
	else if (mul->src2 != GXM_BLEND_REG_O0)
		gxmBlendSetMad(user, GXM_BLEND_OP_MAD, mul->src2, alpha2,
			       mul->src1, alpha1, other, otherAlpha, negate);
	else
		return SCE_FALSE;
	return SCE_TRUE;
}

GxmBlendInstr *gxmBlendOptimize(GxmBlendInstr *first, GxmBlendInstr *end)
{
	GxmBlendInstr *last = end - 1;
	GxmBlendInstr *instr, *user, *prev;
	SceUInt32 state[4], reg, value, reads, mask;
	SceBool kept, used, clobbered, viaSrc1, viaSrc2, usesR0, usesR1;

	/* 1: copies and constants */
	instr = first;
	while (instr < last) {
		if (!gxmBlendIsCopy(instr, &value)) {
			instr++;
			continue;
		}
		reg = instr->dst;
		kept = reg == GXM_BLEND_REG_O0;
		gxmBlendStateInit(state, instr->mask);
		for (user = instr + 1; user < end; user++) {
			if (gxmBlendReadsAll(user, user->src0, user->src0Alpha, reg, state) ||
			    gxmBlendReadsAll(user, user->src1, user->src1Alpha, reg, state) ||
			    gxmBlendReadsAll(user, user->src2, user->src2Alpha, reg, state)) {
				gxmBlendSubstitute(user, reg, value, &kept);
				continue;
			}
			if (gxmBlendReadsAny(user, user->src0, user->src0Alpha, reg, state) ||
			    gxmBlendReadsAny(user, user->src1, user->src1Alpha, reg, state) ||
			    gxmBlendReadsAny(user, user->src2, user->src2Alpha, reg, state))
				kept = SCE_TRUE;
			if (user->dst == reg)
				gxmBlendStateSet(state, user->mask, GXM_BLEND_OVERWRITTEN);
		}
		if (!kept) {
			gxmBlendRemove(instr, end);
			end = last--;
			continue;
		}
		if ((reg != GXM_BLEND_REG_O0 && value == GXM_BLEND_REG_O0) || instr->negate) {
			instr++;
			continue;
		}

		/* let the operations computing value write reg directly */
		gxmBlendStateInit(state, instr->mask);
		for (prev = instr - 1; prev >= first; prev--) {
			if (prev->dst == value && gxmBlendCovers(state, prev->mask)) {
				prev->dst = reg;
				gxmBlendStateSet(state, prev->mask, GXM_BLEND_OVERWRITTEN);
			}
			reads = 0;
			if (prev->src0 == reg)
				reads = gxmBlendReadMask(prev, prev->src0Alpha);
			if (prev->src1 == reg)
				reads |= gxmBlendReadMask(prev, prev->src1Alpha);
			if (prev->src2 == reg)
				reads |= gxmBlendReadMask(prev, prev->src2Alpha);
			if (reads & 1)
				state[0] |= GXM_BLEND_READ;
			if (reads & 2)
				state[1] |= GXM_BLEND_READ;
			if (reads & 4)
				state[2] |= GXM_BLEND_READ;
			if (reads & 8)
				state[3] |= GXM_BLEND_READ;
		}
		mask = ((state[1] & GXM_BLEND_PENDING) ? 2 : 0) | (state[0] & GXM_BLEND_PENDING) |
		       ((state[2] & GXM_BLEND_PENDING) ? 4 : 0) |
		       ((state[3] & GXM_BLEND_PENDING) ? 8 : 0);
		if (mask) {
			instr->mask = mask;
			instr++;
			continue;
		}
		gxmBlendRemove(instr, end);
		end = last--;
	}

	/* 2: multiplies feeding adds */
	instr = first;
	while (instr < last) {
		reg = instr->dst;
		if (instr->op != GXM_BLEND_OP_MUL || reg == instr->src1 || reg == instr->src2 ||
		    reg == GXM_BLEND_REG_O0) {
			instr++;
			continue;
		}
		gxmBlendStateInit(state, instr->mask);
		used = SCE_FALSE;
		clobbered = SCE_FALSE;
		for (user = instr + 1; user < end; user++) {
			if (!clobbered && user->op == GXM_BLEND_OP_ADD &&
			    (gxmBlendReadsAll(user, user->src1, user->src1Alpha, reg, state) ||
			     gxmBlendReadsAll(user, user->src2, user->src2Alpha, reg, state))) {
				viaSrc1 = gxmBlendReadsAll(user, user->src1, user->src1Alpha, reg,
							   state);
				viaSrc2 = gxmBlendReadsAll(user, user->src2, user->src2Alpha, reg,
							   state) && !user->negate;
				if (!(user->mask == 0xc || user->mask == 0xf || user->mask == 3) ||
				    !(viaSrc1 || viaSrc2) ||
				    !gxmBlendFoldMultiply(user, instr, viaSrc1))
					used = SCE_TRUE;
			} else if (gxmBlendReadsAny(user, user->src0, user->src0Alpha, reg, state) ||
				   gxmBlendReadsAny(user, user->src1, user->src1Alpha, reg, state) ||
				   gxmBlendReadsAny(user, user->src2, user->src2Alpha, reg, state)) {
				used = SCE_TRUE;
			} else if (gxmBlendClobbers(user, instr, instr->src0, instr->src0Alpha) ||
				   gxmBlendClobbers(user, instr, instr->src1, instr->src1Alpha) ||
				   gxmBlendClobbers(user, instr, instr->src2, instr->src2Alpha)) {
				clobbered = SCE_TRUE;
			}
			if (user->dst == reg)
				gxmBlendStateSet(state, user->mask, GXM_BLEND_OVERWRITTEN);
		}
		if (used) {
			instr++;
			continue;
		}
		gxmBlendRemove(instr, end);
		end = last--;
	}

	/* 3: use r0 rather than r1 when r0 is free */
	usesR0 = SCE_FALSE;
	usesR1 = SCE_FALSE;
	for (instr = first; instr < end; instr++) {
		usesR0 |= instr->dst == GXM_BLEND_REG_R0 || instr->src0 == GXM_BLEND_REG_R0 ||
			  instr->src1 == GXM_BLEND_REG_R0 || instr->src2 == GXM_BLEND_REG_R0;
		usesR1 |= instr->dst == GXM_BLEND_REG_R1 || instr->src0 == GXM_BLEND_REG_R1 ||
			  instr->src1 == GXM_BLEND_REG_R1 || instr->src2 == GXM_BLEND_REG_R1;
	}
	if (usesR1 && !usesR0) {
		for (instr = first; instr < end; instr++) {
			if (instr->dst == GXM_BLEND_REG_R1)
				instr->dst = GXM_BLEND_REG_R0;
			if (instr->src0 == GXM_BLEND_REG_R1)
				instr->src0 = GXM_BLEND_REG_R0;
			if (instr->src1 == GXM_BLEND_REG_R1)
				instr->src1 = GXM_BLEND_REG_R0;
			if (instr->src2 == GXM_BLEND_REG_R1)
				instr->src2 = GXM_BLEND_REG_R0;
		}
	}
	return end;
}
