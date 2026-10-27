/*
 * relocator.c - SCE relocation formats 0-9 (PT_SCE_RELA)
 */

#include "modulemgr_internal.h"

typedef struct RelocSegment {
	SceUInt32 vaddr;		/* p_vaddr */
	SceUInt32 base;			/* load address */
	SceSize filesz;
	SceSize memsz;
	SceUInt8 *kaddr;		/* kernel mapping */
} RelocSegment;

SceUInt32 g_relocFormCount[12];
SceUInt32 g_relocForms;
LoadelfTiming *g_loadTiming;

/* the segments the relocations refer to, with their count in front */
typedef struct RelocContext {
	SceUInt32 nseg;
	RelocSegment seg[4];
} RelocContext;

/* the debug build keeps it out of line */
#if FW_DEBUG
static __attribute__((noinline)) int get_addend(SceUInt32 patchSeg, SceUInt32 offset, RelocContext *rc,
						SceUInt32 *addend, SceUInt32 *symSeg)
#else
static inline int get_addend(SceUInt32 patchSeg, SceUInt32 offset, RelocContext *rc,
			     SceUInt32 *addend, SceUInt32 *symSeg)
#endif
{
	SceUInt32 data = *(SceUInt32 *)(rc->seg[patchSeg].kaddr + offset);
	SceUInt32 i;

	for (i = 0; i < rc->nseg; i++) {
		if (rc->seg[i].base != 0 && rc->seg[i].vaddr <= data &&
		    data < rc->seg[i].memsz + rc->seg[i].vaddr) {
			*addend = data - rc->seg[i].vaddr;
			*symSeg = i;
			return 1;
		}
	}
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 152, "pseg:r_offset = %d:%08x data = %08x not found segment\n",
			     patchSeg, offset, data);
	return 0;
}

/* the fields of the instructions the relocations patch */
typedef union RelocInsn {
	SceUInt32 word;
	struct {			/* ARM MOVW/MOVT */
		SceUInt32 imm12 : 12;
		SceUInt32 rd : 4;
		SceUInt32 imm4 : 4;
		SceUInt32 opcode : 12;
	} movw;
	struct {			/* Thumb-2 MOVW/MOVT */
		SceUInt32 imm4 : 4;
		SceUInt32 opcode1 : 6;
		SceUInt32 i : 1;
		SceUInt32 opcode2 : 5;
		SceUInt32 imm8 : 8;
		SceUInt32 rd : 4;
		SceUInt32 imm3 : 3;
		SceUInt32 opcode3 : 1;
	} thumbMovw;
	struct {			/* Thumb-2 BL/BLX */
		SceUInt32 imm10 : 10;
		SceUInt32 s : 1;
		SceUInt32 opcode1 : 5;
		SceUInt32 imm11 : 11;
		SceUInt32 j2 : 1;
		SceUInt32 bl : 1;	/* 0: BLX */
		SceUInt32 j1 : 1;
		SceUInt32 opcode2 : 2;
	} thumbBl;
	struct {			/* ARM B/BL */
		SceUInt32 imm24 : 24;
		SceUInt32 opcode : 4;
		SceUInt32 cond : 4;
	} b;
	struct {			/* ARM BLX: the H bit */
		SceUInt32 imm23 : 23;
		SceUInt32 h : 2;
		SceUInt32 opcode : 7;
	} blx;
} RelocInsn;

int apply_one_relocation(SceUInt32 *where, SceUInt32 type, SceUInt32 offset,
				SceUInt32 S, SceUInt32 A, SceUInt32 base)
{
	RelocInsn insn;
	SceUInt32 v, sign, imm11, j1, j2, imm10;
	int ret;

	switch (type) {
	case 0:		/* R_ARM_NONE */
	case 0x28:	/* R_ARM_V4BX */
		ret = 0;
		break;
	case 2:		/* R_ARM_ABS32 */
	case 0x26:	/* R_ARM_TARGET1 */
		*where = S + A;
		ret = 0;
		break;
	case 3:		/* R_ARM_REL32 */
	case 0x29:	/* R_ARM_TARGET2 */
		*where = S - (offset + base) + A;
		ret = 0;
		break;
	case 10:	/* R_ARM_THM_CALL */
		v = S - (offset + base) + A;
		insn.word = *where;
		sign = (v >> 24) & 1;
		j1 = sign == ((v >> 23) & 1);
		j2 = sign == ((v >> 22) & 1);
		imm10 = v >> 12;
		imm11 = (v >> 1) & 0x7ff;
		if (insn.thumbBl.bl == 0)
			imm11 &= ~1u;
		insn.thumbBl.j1 = j1;
		insn.thumbBl.j2 = j2;
		insn.thumbBl.imm11 = imm11;
		insn.thumbBl.s = sign;
		insn.thumbBl.imm10 = imm10;
		*where = insn.word;
		ret = 0;
		break;
	case 0x1c:	/* R_ARM_CALL */
	case 0x1d:	/* R_ARM_JUMP24 */
		insn.word = *where;
		v = S - (offset + base) + A;
		if (insn.b.cond == 0)
			insn.blx.h = v;
		insn.b.imm24 = v >> 2;
		*where = insn.word;
		ret = 0;
		break;
	case 0x2a:	/* R_ARM_PREL31; Sony's mask is decimal 80000000 */
		*where = (*where & 80000000) | ((S - (offset + base) + A) & 0x7fffffff);
		ret = 0;
		break;
	case 0x2b:	/* R_ARM_MOVW_ABS_NC */
		v = S + A;
		insn.word = *where;
		insn.movw.imm12 = v;
		insn.movw.imm4 = v >> 12;
		*where = insn.word;
		ret = 0;
		break;
	case 0x2c:	/* R_ARM_MOVT_ABS */
		v = S + A;
		insn.word = *where;
		insn.movw.imm12 = v >> 16;
		insn.movw.imm4 = v >> 28;
		*where = insn.word;
		ret = 0;
		break;
	case 0x2f:	/* R_ARM_THM_MOVW_ABS_NC */
		v = S + A;
		insn.word = *where;
		insn.thumbMovw.imm8 = v;
		insn.thumbMovw.imm3 = v >> 8;
		insn.thumbMovw.i = v >> 11;
		insn.thumbMovw.imm4 = v >> 12;
		*where = insn.word;
		ret = 0;
		break;
	case 0x30:	/* R_ARM_THM_MOVT_ABS */
		v = S + A;
		insn.word = *where;
		insn.thumbMovw.imm8 = v >> 16;
		insn.thumbMovw.imm3 = v >> 24;
		insn.thumbMovw.i = v >> 27;
		insn.thumbMovw.imm4 = v >> 28;
		*where = insn.word;
		ret = 0;
		break;
	default:
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 85,
			"Unsupported r_type(%d) r_offset=0x%08x, S=0x%08x, A=0x%08x, P=0x%08x\n",
			type, offset, S, A, offset + base);
		ret = SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
		break;
	}
	return ret;
}

#define IS_MOVW_MOVT(t1, t2)	(((t1) == 0x2f && (t2) == 0x30) || ((t1) == 0x2b && (t2) == 0x2c))

int RelocatorRelSegment(LoadelfContext *ctx, SceUInt32 *rel, SceSize size)
{
	SceModuleCB *cb = ctx->cb;
	RelocContext rc;
	SceUInt32 *end;
	SceUInt32 h, form, step, patchSeg, symSeg, addend, type1, type2;
	SceUInt32 offset, offset2, offset3, d1, d2, d3, d4, bits, width;
	SceUInt32 S, P, limit;
	SceUInt8 *kaddr;
	int i;

	rc.nseg = 0;
	for (i = 0; i < 4; i++) {
		if (i < cb->nSegments) {
			rc.nseg = i + 1;
			rc.seg[i].vaddr = ctx->seg[i].vaddr;
			rc.seg[i].base = (SceUInt32)cb->segment[i].vaddr;
			rc.seg[i].filesz = cb->segment[i].filesz;
			rc.seg[i].memsz = cb->segment[i].memsz;
			rc.seg[i].kaddr = ctx->seg[i].kaddr;
		} else {
			rc.seg[i].vaddr = 0;
			rc.seg[i].base = 0;
			rc.seg[i].filesz = 0;
			rc.seg[i].memsz = 0;
			rc.seg[i].kaddr = NULL;
		}
	}

	h = *rel;
	if ((h & 0xf) > 1) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 199, "Relocation Error! Segment top must form0 or form1");
		return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
	}

	end = (SceUInt32 *)((SceUInt8 *)rel + size);
	offset = 0;
	symSeg = 0;
	addend = 0;
	type1 = 0;
	type2 = 0;
	patchSeg = 0;
	S = rc.seg[0].base;
	P = rc.seg[0].base;
	limit = rc.seg[0].filesz;
	kaddr = rc.seg[0].kaddr;

	while (rel < end) {
		form = h & 0xf;
		__builtin_prefetch(rel + 8);
		switch (form) {
		case 0:
		case 1:
			if (form == 0) {
				addend = rel[1];
				offset = rel[2];
			} else {
				addend = (SceInt32)rel[1] >> 10;
				offset = h >> 20 | (rel[1] & 0x3ff) << 12;
			}
			symSeg = (h << 24) >> 28;
			type1 = (h << 16) >> 24;
			d1 = (h << 12) >> 28;
			if (d1 >= rc.nseg) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 411, "wrong patch segment (%d)\n", patchSeg);
				return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
			}
			if (symSeg >= rc.nseg)
				goto wrong_sym;
			if (d1 != patchSeg) {
				limit = rc.seg[d1].filesz;
				kaddr = rc.seg[d1].kaddr;
				P = rc.seg[d1].base;
				patchSeg = d1;
				if (limit < 4)
					goto wrong_offset;
			}
			S = rc.seg[symSeg].base;
			if (offset > limit - 4)
				goto wrong_offset;
			apply_one_relocation((SceUInt32 *)(kaddr + offset), type1, offset, S, addend, P);
			if (form == 1) {
				step = 8;
				type2 = 0;
				g_relocForms |= 4;
				break;
			}
			type2 = (h << 5) >> 25;
			if (type2 == 0) {
				step = 12;
				g_relocForms |= 1;
				break;
			}
			if (h >> 27 == 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 231, "form0 - r_type2 or dist_2 invalid\n");
				return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
			}
			offset2 = offset + (h >> 27);
			if (offset2 > limit - 4)
				goto wrong_offset;
			apply_one_relocation((SceUInt32 *)(kaddr + offset2), type2, offset2,
					     S, addend, P);
			step = 12;
			g_relocForms |= 2;
			break;
		case 2:
			symSeg = (h << 24) >> 28;
			addend = rel[1];
			type1 = (h << 16) >> 24;
			if (symSeg >= rc.nseg)
				goto wrong_sym;
			S = rc.seg[symSeg].base;
			if (h >> 16 == 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 274, "form2 - dist_1 == 0\n");
				return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
			}
			offset += h >> 16;
			if (offset > limit - 4)
				goto wrong_offset;
			apply_one_relocation((SceUInt32 *)(kaddr + offset), type1, offset, S, addend, P);
			step = 8;
			type2 = 0;
			g_relocForms |= 8;
			break;
		case 3:
			symSeg = (h << 24) >> 28;
			addend = rel[1];
			d1 = (h << 5) >> 14;
			if (symSeg >= rc.nseg)
				goto wrong_sym;
			if ((h & 0x100) == 0) {
				type2 = 0x2c;
				type1 = 0x2b;
			} else {
				type2 = 0x30;
				type1 = 0x2f;
			}
			S = rc.seg[symSeg].base;
			if (h >> 27 == 0 || d1 == 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 299, "form3 - dist_1 == 0 or dist_2 == 0\n");
				return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
			}
			offset += d1;
			if (offset > limit - 4)
				goto wrong_offset;
			apply_one_relocation((SceUInt32 *)(kaddr + offset), type1, offset, S, addend, P);
			offset2 = offset + (h >> 27);
			if (offset2 > limit - 4)
				goto wrong_offset;
			apply_one_relocation((SceUInt32 *)(kaddr + offset2), type2, offset2,
					     S, addend, P);
			step = 8;
			g_relocForms |= 0x10;
			break;
		case 4:
			d1 = (h << 5) >> 9;
			if (d1 == 0 || h >> 27 == 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 314, "form4 - dist_1 == 0 or dist_2 == 0\n");
				return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
			}
			if (!IS_MOVW_MOVT(type1, type2)) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 320, "form4 - invalid r_type1,r_type2 %d,%d\n",
						     type1, type2);
				return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
			}
			offset += d1;
			if (offset > limit - 4)
				goto wrong_offset;
			apply_one_relocation((SceUInt32 *)(kaddr + offset), type1, offset, S, addend, P);
			offset2 = offset + (h >> 27);
			if (offset2 > limit - 4)
				goto wrong_offset;
			apply_one_relocation((SceUInt32 *)(kaddr + offset2), type2, offset2,
					     S, addend, P);
			step = 4;
			g_relocForms |= 0x20;
			break;
		case 5:
			d1 = (h << 19) >> 23;
			d2 = (h << 14) >> 27;
			d3 = (h << 5) >> 23;
			d4 = h >> 27;
			if (d2 == 0 || d1 == 0 || d4 == 0 || d3 == 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 337, "form5 - dist_x == 0\n");
				return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
			}
			if (!IS_MOVW_MOVT(type1, type2)) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 343, "form5 - invalid r_type1,r_type2 %d, %d\n",
						     type1, type2);
				return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
			}
			offset += d1;
			if (offset > limit - 4)
				goto wrong_offset;
			apply_one_relocation((SceUInt32 *)(kaddr + offset), type1, offset, S, addend, P);
			offset2 = offset + d2;
			if (offset2 > limit - 4)
				goto wrong_offset;
			offset += d3;
			apply_one_relocation((SceUInt32 *)(kaddr + offset2), type2, offset2,
					     S, addend, P);
			if (offset > limit - 4)
				goto wrong_offset;
			apply_one_relocation((SceUInt32 *)(kaddr + offset), type1, offset, S, addend, P);
			offset3 = offset + d4;
			if (offset3 > limit - 4)
				goto wrong_offset;
			apply_one_relocation((SceUInt32 *)(kaddr + offset3), type2, offset3,
					     S, addend, P);
			step = 4;
			g_relocForms |= 0x40;
			break;
		case 6:
			if (h >> 4 == 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 364, "form6 - dist_1 == 0\n");
				return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
			}
			offset += h >> 4;
			if (!get_addend(patchSeg, offset, &rc, &addend, &symSeg))
				return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
			S = rc.seg[symSeg].base;
			if (offset > limit - 4)
				goto wrong_offset;
			type2 = 0;
			type1 = 2;
			apply_one_relocation((SceUInt32 *)(kaddr + offset), 2, offset, S, addend, P);
			step = 4;
			g_relocForms |= 0x100;
			break;
		case 7:
		case 8:
		case 9:
			bits = h >> 4;
			width = (0x1e2 >> ((27 - form * 3) & 0xff)) & 7;
			do {
				d1 = (bits << (32 - width)) >> (32 - width);
				if (d1 == 0) {
					SCE_DBG_PRINTF_LEVEL(1, 0xb, 388, "form%d - dist_x == 0\n", form);
					return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
				}
				offset += d1 * 4;
				if (!get_addend(patchSeg, offset, &rc, &addend, &symSeg))
					return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
				S = rc.seg[symSeg].base;
				if (offset > limit - 4)
					goto wrong_offset;
				apply_one_relocation((SceUInt32 *)(kaddr + offset), 2, offset, S, addend, P);
				bits >>= width;
			} while (bits != 0);
			type1 = 2;
			step = 4;
			g_relocForms |= 1u << (form + 2);
			type2 = 0;
			break;
		default:
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 401, "illegal form code (%d)\n", form);
			return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
		}
		rel = (SceUInt32 *)((SceUInt8 *)rel + step);
		if (rel >= end)
			break;
		h = *rel;
	}
	return 0;

wrong_offset:
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 408, "wrong r_offset (0x%08x)\n", offset);
	return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
wrong_sym:
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 414, "wrong sym segment (%d)\n", symSeg);
	return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REL_INFO;
}

/* name unknown: count the forms used by the last module */
void relocatorFormCount(void)
{
	SceUInt32 bit = 1;
	int i;

	for (i = 0; i < 12; i++) {
		if ((bit & g_relocForms) != 0)
			g_relocFormCount[i]++;
		bit <<= 1;
	}
	g_relocForms = 0;
}

#if !MODULEMGR_NSKBL
void relocatorFormLogPrint(void)
{
	SCE_DBG_PRINTF_LEVEL(1, 0, 442, "Relocation Forms [%d, %d] %d %d, %d %d %d, %d %d %d %d\n\n",
			     g_relocFormCount[0], g_relocFormCount[1], g_relocFormCount[2],
			     g_relocFormCount[3], g_relocFormCount[4], g_relocFormCount[5],
			     g_relocFormCount[6], g_relocFormCount[8], g_relocFormCount[9],
			     g_relocFormCount[10], g_relocFormCount[11]);
	g_relocFormCount[0] = 0;
	g_relocFormCount[1] = 0;
	g_relocFormCount[2] = 0;
	g_relocFormCount[3] = 0;
	g_relocFormCount[4] = 0;
	g_relocFormCount[5] = 0;
	g_relocFormCount[6] = 0;
	g_relocFormCount[7] = 0;
	g_relocFormCount[8] = 0;
	g_relocFormCount[9] = 0;
	g_relocFormCount[10] = 0;
	g_relocFormCount[11] = 0;
	g_relocForms = 0;
}
#endif
