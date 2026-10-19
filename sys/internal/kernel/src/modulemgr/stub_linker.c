/*
 * stub_linker.c - writing the import stubs of a module and applying the
 * reference tables (relocations) of imported variables and functions
 */

#include "modulemgr_internal.h"
#include <excpmgr/excpmgr_cpu.h>

/* DACR with every domain in manager mode: kernel text is writable */
#define STUBLINKER_DACR_WRITABLE	0xffff0000

/* link modes */
#define STUBLINKER_LINK			0
#define STUBLINKER_UNLINK		1	/* back to "no entry" */
#define STUBLINKER_UNDEF		2	/* trap */

/* reference table relocation types (ELF R_ARM_*) */
#define R_ARM_ABS32			2
#define R_ARM_REL32			3
#define R_ARM_THM_CALL			10
#define R_ARM_CALL			28
#define R_ARM_JUMP24			29
#define R_ARM_TARGET1			38
#define R_ARM_TARGET2			41
#define R_ARM_PREL31			42
#define R_ARM_MOVW_ABS_NC		43
#define R_ARM_MOVT_ABS			44
#define R_ARM_THM_MOVW_ABS_NC		47
#define R_ARM_THM_MOVT_ABS		48
#define R_ARM_NONE_SCE			0xff

#define ARM_BKPT_MASK			0x0ff000f0
#define ARM_BKPT			0x01200070
#define THUMB_BKPT_MASK			0xff00
#define THUMB_BKPT			0xbe00

/* the instructions a function stub holds */
#define STUB_MOVW_R12			0xe300c000
#define STUB_MOVT_R12			0xe340c000
#define STUB_BX_R12			0xe12fff1c
#define STUB_SVC_0			0xef000000
#define STUB_BX_LR			0xe12fff1e
#define STUB_SUB_R12_PC_8		0xe24fc008	/* unlinked: sub r12, pc, #8 */
#define STUB_MOV_PC_0			0xe3a0f000	/* unlinked: mov pc, #0 */
#define STUB_MVN_R0_0			0xe3e00000	/* as linked by the toolchain */
#define STUB_BKPT			0xe12cd070

/* the reference table of an import: a header word and then the entries */
#define REFTABLE_SIZE(hdr)		(((hdr) >> 4) & 0xffffff)
#define REFTABLE_HDR_MASK		0xf000000f

/* an instruction read for patching, with what was in memory */
typedef struct StubLinkerInsn {
	SceUInt32 insn;			/* breakpoints replaced by the original */
	SceUInt32 orig;
} StubLinkerInsn;

/* name unknown */
int StubLinkerCopyIn(SceUID pid, void *dst, const void *src, SceSize len)
{
	if (pid != KERNEL_PID)
		return sceKernelCopyFromUser(dst, src, len);
	memcpy(dst, src, len);
	return 0;
}

/* name unknown: read a Thumb-2 instruction from under any breakpoints */
int StubLinkerReadThumb(SceUID pid, SceUInt32 addr, StubLinkerInsn *in, int debugger)
{
	SceUInt16 *half = (SceUInt16 *)&in->insn;
	SceUInt32 orig;
	int ret, i;

	ret = StubLinkerCopyIn(pid, &in->insn, (const void *)addr, 4);
	if (ret < 0)
		return ret;
	in->orig = in->insn;
	if (debugger) {
		for (i = 0; i < 2; i++) {
			if ((((SceUInt16 *)&in->orig)[i] & THUMB_BKPT_MASK) == THUMB_BKPT &&
			    SceSysrootForKernel_9CFF80F9(pid, (void *)addr, &orig) == 0)
				half[i] = orig;
		}
	}
	return 0;
}

/* name unknown: read an ARM instruction from under a breakpoint */
int StubLinkerReadArm(SceUID pid, SceUInt32 addr, StubLinkerInsn *in, int debugger)
{
	SceUInt32 orig;
	int ret;

	ret = StubLinkerCopyIn(pid, &in->insn, (const void *)addr, 4);
	if (ret < 0)
		return ret;
	in->orig = in->insn;
	if (debugger && (in->insn & ARM_BKPT_MASK) == ARM_BKPT &&
	    SceSysrootForKernel_9CFF80F9(pid, (void *)addr, &orig) == 0)
		in->insn = orig;
	return 0;
}

/* keep the breakpoints of a debugger over a patched Thumb-2 instruction */
static inline void StubLinkerKeepThumbBkpt(SceUID pid, SceUInt32 addr, StubLinkerInsn *in)
{
	SceUInt16 *half = (SceUInt16 *)&in->insn, *orig = (SceUInt16 *)&in->orig;
	int i;

	for (i = 0; i < 2; i++) {
		if ((orig[i] & THUMB_BKPT_MASK) == THUMB_BKPT) {
			SceSysrootForKernel_38751FB7(pid, addr, half[i]);
			half[i] = orig[i];
		}
	}
}

static inline void StubLinkerWriteKernelWord(SceUInt32 *dst, SceUInt32 value)
{
	SceUInt32 dacr, start = (SceUInt32)dst & ~0x1f;

	dacr = readDacr();
	writeDacr(STUBLINKER_DACR_WRITABLE);
	*dst = value;
	sceKernelDcacheCleanRangeForL1WBWA((void *)start,
					    (((SceUInt32)dst + 4 + 0x1f) & ~0x1f) - start);
	writeDacr(dacr);
}

int apply_reftable(SceModuleCB *cb, const void *reftable, SceUInt32 value, int mode)
{
	SceUID pid = cb->pid, map = 0;
	SceUInt8 buf[0x200];
	const SceUInt32 *p, *next, *end;
	SceModuleSegment *seg = NULL;
	SceUInt32 hdr, w, segno, last, memsz, type, offset, v;
	SceUInt32 *where;
	StubLinkerInsn in;
	SceInt32 addend;
	void *kbase;
	SceSize ksize;
	SceUInt32 koffset;
	int debugger, ret;

	debugger = SceSysrootForKernel_6050A467(pid);
	if (!ModuleCBIsInReadOnly(cb, (SceUInt32)reftable, 4))
		return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REF_INFO;
	ret = StubLinkerCopyIn(pid, &hdr, reftable, 4);
	if (ret < 0 || (hdr & REFTABLE_HDR_MASK) != 0)
		return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REF_INFO;
	if (!ModuleCBIsInReadOnly(cb, (SceUInt32)reftable, REFTABLE_SIZE(hdr)))
		return SCE_KERNEL_ERROR_MODULEMGR_INVALID_REF_INFO;

	if (pid != KERNEL_PID) {
		if (REFTABLE_SIZE(hdr) < sizeof(buf)) {
			ret = StubLinkerCopyIn(pid, buf, reftable, REFTABLE_SIZE(hdr));
			if (ret < 0)
				return ret;
			reftable = buf;
		} else {
			ret = SceSysmemForDriver_0091D74D(pid, "SceModuleMgrRelocation", 1, reftable,
						    REFTABLE_SIZE(hdr), &kbase, &ksize, &koffset);
			if (ret < 0)
				return ret;
			map = ret;
			reftable = (SceUInt8 *)kbase + koffset;
		}
	}

	debugger = debugger == 1;
	end = (const SceUInt32 *)((const SceUInt8 *)reftable + REFTABLE_SIZE(hdr));
	last = 0xffff;
	p = (const SceUInt32 *)reftable + 1;
	for (;;) {
		next = p + 2;
		if (next > end) {
			ret = p == end ? 0 : SCE_KERNEL_ERROR_MODULEMGR_INVALID_REF_INFO;
			break;
		}
		w = p[0];
		if ((w & 0xf) == 1) {
			addend = (SceInt32)w >> 16;
		} else if ((w & 0xf) == 2) {
			if (p + 3 > end) {
				ret = SCE_KERNEL_ERROR_MODULEMGR_INVALID_REF_INFO;
				break;
			}
			addend = p[2];
			next = p + 3;
		} else {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 869, "unknown form=%d, addr=0x%08x\n", w & 0xf,
					     next);
			ret = SCE_KERNEL_ERROR_MODULEMGR_INVALID_REF_INFO;
			break;
		}

		segno = (w >> 4) & 0xf;
		if (segno == 0xf) {
			ret = SCE_KERNEL_ERROR_MODULEMGR_INVALID_REF_INFO;
			break;
		}
		if (segno != last) {
			if ((int)segno >= cb->nSegments) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 882, "not found a segment (%d)\n", segno);
				ret = SCE_KERNEL_ERROR_MODULEMGR_INVALID_REF_INFO;
				break;
			}
			seg = &cb->segment[segno];
			memsz = seg->memsz;
			if (memsz < 4) {
				ret = SCE_KERNEL_ERROR_MODULEMGR_INVALID_REF_INFO;
				break;
			}
		} else {
			memsz = seg->memsz;
		}
		offset = p[1];
		if (offset > memsz - 4) {
			ret = SCE_KERNEL_ERROR_MODULEMGR_INVALID_REF_INFO;
			break;
		}
		type = (w >> 8) & 0xff;
		where = (SceUInt32 *)((SceUInt8 *)seg->vaddr + offset);
		last = segno;
		p = next;

		switch (type) {
		case R_ARM_ABS32:
		case R_ARM_TARGET1:
			in.insn = mode != 0 ? 0 : addend + value;
			goto data;
		case R_ARM_REL32:
		case R_ARM_TARGET2:
			in.insn = mode != 0 ? 0 : addend - (SceUInt32)where + value;
			goto data;
		case R_ARM_PREL31:
			in.insn = mode != 0 ? 0 : (addend - (SceUInt32)where + value) & 0x7fffffff;
data:
			if (pid == KERNEL_PID) {
				StubLinkerWriteKernelWord(where, in.insn);
				ret = 0;
				continue;
			}
			ret = SceSysmemForKernel_499A9FFF(where, &in.insn, 4);
			break;

		case R_ARM_THM_CALL:
			ret = StubLinkerReadThumb(pid, (SceUInt32)where, &in, debugger);
			if (ret < 0)
				goto out;
			v = addend - (SceUInt32)where + value;
			{
				SceUInt32 s = (v >> 24) & 1, i1 = (v >> 23) & 1, i2 = (v >> 22) & 1;
				SceUInt32 imm11 = (v >> 1) & 0x7ff;

				if ((in.insn & 0x10000000) == 0)	/* BLX */
					imm11 &= ~1;
				in.insn = (in.insn & 0xd000f800) | (s == i1) << 29 | (s == i2) << 27 |
					  imm11 << 16 | s << 10 | ((v >> 12) & 0x3ff);
			}
			if (debugger)
				StubLinkerKeepThumbBkpt(pid, (SceUInt32)where, &in);
			goto text;

		case R_ARM_CALL:
		case R_ARM_JUMP24:
			ret = StubLinkerReadArm(pid, (SceUInt32)where, &in, debugger);
			if (ret < 0)
				goto out;
			v = addend - (SceUInt32)where + value;
			if ((in.insn & 0xf0000000) == 0)
				in.insn = (in.insn & 0xfe7fffff) | (v & 3) << 23;
			in.insn = (in.insn & 0xff000000) | ((v >> 2) & 0xffffff);
			goto arm;

		case R_ARM_MOVW_ABS_NC:
			ret = StubLinkerReadArm(pid, (SceUInt32)where, &in, debugger);
			if (ret < 0)
				goto out;
			v = mode != 0 ? 0 : addend + value;
			in.insn = (in.insn & 0xfff0f000) | (v & 0xfff) | ((v >> 12) & 0xf) << 16;
			goto arm;

		case R_ARM_MOVT_ABS:
			ret = StubLinkerReadArm(pid, (SceUInt32)where, &in, debugger);
			if (ret < 0)
				goto out;
			v = mode != 0 ? 0 : addend + value;
			in.insn = (in.insn & 0xfff0f000) | ((v >> 16) & 0xfff) | (v >> 28) << 16;
arm:
			if (debugger && (in.orig & ARM_BKPT_MASK) == ARM_BKPT) {
				SceSysrootForKernel_38751FB7(pid, (SceUInt32)where, in.insn);
				in.insn = in.orig;
			}
			goto text;

		case R_ARM_THM_MOVW_ABS_NC:
			ret = StubLinkerReadThumb(pid, (SceUInt32)where, &in, debugger);
			if (ret < 0)
				goto out;
			v = mode != 0 ? 0 : addend + value;
			in.insn = (in.insn & 0x8f00fbf0) | (v & 0xff) << 16 | ((v >> 8) & 7) << 28 |
				  ((v >> 11) & 1) << 10 | ((v >> 12) & 0xf);
			if (debugger)
				StubLinkerKeepThumbBkpt(pid, (SceUInt32)where, &in);
			goto text;

		case R_ARM_THM_MOVT_ABS:
			ret = StubLinkerReadThumb(pid, (SceUInt32)where, &in, debugger);
			if (ret < 0)
				goto out;
			v = mode != 0 ? 0 : addend + value;
			in.insn = (in.insn & 0x8f00fbf0) | ((v >> 16) & 0xff) << 16 |
				  ((v >> 24) & 7) << 28 | ((v >> 27) & 1) << 10 | (v >> 28);
			if (debugger)
				StubLinkerKeepThumbBkpt(pid, (SceUInt32)where, &in);
text:
			if (pid == KERNEL_PID) {
				StubLinkerWriteKernelWord(where, in.insn);
				ret = 0;
				continue;
			}
			ret = SceSysmemForKernel_5EF1DAAF(where, &in.insn, 4);
			break;

		case R_ARM_NONE_SCE:
			break;

		default:
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 933, "Unknown rel type (%d)\n", type);
			ret = SCE_KERNEL_ERROR_MODULEMGR_INVALID_REF_INFO;
			goto out;
		}
		if (ret < 0)
			break;
	}
out:
	if (map != 0)
		sceKernelUserUnmap(map, NULL, 0);
	return ret;
}

/* look a NID up in the hashed NID table of the library */
static inline SceUInt32 StubLinkerFindNid(const SceUInt16 *hash, SceUInt32 bits, const SceNID *nids,
					  SceUInt32 n, SceNID nid)
{
	SceUInt32 i = 0;

	if (bits != 0)
		i = hash[nid >> bits];
	for (; i < n; i++) {
		if (nids[i] == nid)
			break;
	}
	return i;
}

int link_variables(StubLinkerInfo *info, int mode)
{
	LibStubCB *stub = info->stub.libStubCB;
	LibEntCB *ent = info->ent.libEntCB;
	SceUInt32 i, n = info->stub.nvar, idx;
	const SceNID *nids;
	SceModuleExports *exp;
	void *reftable;
	SceNID nid;
	int ret;

	for (i = 0; i < n; i++) {
		reftable = info->stub.varent[i];
		if (reftable == NULL)
			continue;
		if (mode != STUBLINKER_LINK) {
			ret = apply_reftable(stub->cb, reftable, 0, mode);
		} else {
			nid = info->stub.varnid[i];
			nids = info->ent.nidtable + info->ent.nfunc;
			idx = StubLinkerFindNid(info->ent.varHashTable, info->ent.varHashBits, nids,
						info->ent.nvar, nid);
			if (idx < info->ent.nvar) {
				ret = apply_reftable(stub->cb, reftable,
						     (SceUInt32)info->ent.entrytable[info->ent.nfunc + idx],
						     STUBLINKER_LINK);
			} else {
				exp = ent->exports;
				SCE_PRINTF_WITH_INFO(0, 989,
					"# Library variable entry not found: [nid=0x%08x] in [%s, ver=%d] (ignored)\n",
					nid, exp->libname, exp->version);
				ret = apply_reftable(stub->cb, reftable, 0, STUBLINKER_UNLINK);
			}
		}
		if (ret < 0)
			return ret;
	}
	return 0;
}

int link_functions(StubLinkerInfo *info, int mode, const ModulemgrLibraryPatch *patch)
{
	LibStubCB *stub = info->stub.libStubCB;
	LibEntCB *ent = info->ent.libEntCB;
	SceUID pid = stub->cb->pid;
	SceUInt32 i, j, n = info->stub.nfunc, idx, num, addr, size, dacr, orig;
	SceUInt32 code[4], cur[4];
	int debugger, kind, segno, offset, ret;
	SceNID nid;

	debugger = SceSysrootForKernel_6050A467(pid);
	if (n == 0)
		return 0;

	if (patch != NULL) {
		for (i = 0; i < n; i++) {
			if (mode != STUBLINKER_LINK)
				continue;
			nid = info->stub.funcnid[i];
			SCE_DBG_PRINTF_LEVEL(2, 0xb, 455, "i=%d/%d/nid=0x%08x\n", i, n, nid);
			for (j = 0; j < patch->count; j++) {
				SCE_DBG_PRINTF_LEVEL(2, 0xb, 457, "npatch=%d/%d, 0x%08x\n", j,
						     patch->count, patch->patch[j].from);
				if (patch->patch[j].from == nid) {
					SCE_DBG_PRINTF_LEVEL(1, 0xb, 460, "0x%08x<=0x%08x\n",
							     patch->patch[j].to, nid);
					info->stub.funcnid[i] = patch->patch[j].to;
					break;
				}
			}
		}
	}

	for (i = 0; i < n; i++) {
		addr = (SceUInt32)info->stub.funcent[i];
		if (mode == STUBLINKER_LINK) {
			nid = info->stub.funcnid[i];
			idx = StubLinkerFindNid(info->ent.hashTable, info->ent.funcHashBits,
						info->ent.nidtable, info->ent.nfunc, nid);
			if (idx >= info->ent.nfunc) {
				segno = -1;
				offset = -1;
				ret = ModuleCBGetSegmentOffset(stub->cb, addr, &segno, (SceUInt32 *)&offset);
				SCE_DBG_ASSERT_LEVEL(0, ret == 0, 485);
				(void)ret;
				kind = STUBLINKER_UNLINK;
				SCE_PRINTF_WITH_INFO(0, 489,
					"# Library function entry not found: [nid=0x%08x, index=%d, seg:offset=%d:0x%08x, fingerprint=0x%08x] in [%s, ver=%d] (ignored)\n",
					nid, i, segno, offset, stub->cb->fingerprint,
					ent->exports->libname, ent->exports->version);
				goto noentry;
			}
			if ((ent->exports->attr & LIBRARY_ATTR_SYSCALL) != 0) {
				num = (idx + ent->syscallIndex) & 0xfff;
				SyscallCBSetEntry(ent, idx);
				code[1] = STUB_SVC_0;
				code[2] = STUB_BX_LR;
			} else {
				num = (SceUInt32)info->ent.entrytable[idx];
				code[2] = STUB_BX_R12;
				code[1] = STUB_MOVT_R12 | ((num << 4) >> 20) | (num >> 28) << 16;
			}
			code[0] = STUB_MOVW_R12 | (num & 0xfff) | ((num >> 12) & 0xf) << 16;
			size = 12;
			kind = STUBLINKER_LINK;
		} else if (mode == STUBLINKER_UNDEF) {
			kind = mode;
			code[0] = STUB_BKPT;
			size = 4;
		} else {
			kind = mode;
noentry:
			size = 8;
			code[0] = STUB_SUB_R12_PC_8;
			code[1] = STUB_MOV_PC_0;
		}

		ret = StubLinkerCopyIn(pid, cur, (const void *)addr, sizeof(cur));
		if (ret < 0)
			return ret;
		if (debugger == 1) {
			for (j = 0; j < sizeof(cur); j += 4) {
				orig = cur[j / 4];
				if ((orig & ARM_BKPT_MASK) != ARM_BKPT)
					continue;
				if (SceSysrootForKernel_9CFF80F9(pid, (void *)(addr + j), &cur[j / 4]) == 0) {
					if (j < size) {
						SceSysrootForKernel_38751FB7(pid, addr + j, code[j / 4]);
						code[j / 4] = orig;
					}
				} else {
					cur[j / 4] = orig;
				}
			}
		}
		if ((cur[0] & ARM_BKPT_MASK) == ARM_BKPT)
			continue;
		if (cur[0] != STUB_SUB_R12_PC_8 && cur[0] != STUB_MVN_R0_0 &&
		    (cur[0] & 0xfff0f000) != STUB_MOVW_R12)
			return SCE_KERNEL_ERROR_MODULEMGR_INVALID_STUB;
		if (pid == KERNEL_PID) {
			dacr = readDacr();
			writeDacr(STUBLINKER_DACR_WRITABLE);
			memcpy((void *)addr, code, size);
			sceKernelDcacheCleanRangeForL1WBWA((void *)(addr & ~0x1f),
				((addr + 0x1f + size) & ~0x1f) - (addr & ~0x1f));
			writeDacr(dacr);
		} else {
			ret = SceSysmemForKernel_5EF1DAAF((void *)addr, code, size);
			if (ret < 0)
				return ret;
		}
		/* the fourth word: the reference table of the function */
		if (kind < STUBLINKER_UNDEF && cur[3] != 0) {
			ret = apply_reftable(stub->cb, (const void *)cur[3], addr, kind);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

int StubLinkerLinkAll(StubLinkerInfo *info, const ModulemgrLibraryPatch *patch)
{
	int ret;

	ret = link_functions(info, STUBLINKER_LINK, patch);
	if (ret >= 0) {
		ret = link_variables(info, STUBLINKER_LINK);
		if (ret < 0 && link_functions(info, STUBLINKER_UNLINK, NULL) < 0)
			SCE_PANIC(1019);
	}
	return ret;
}

/* name unknown */
int StubLinkerUnlinkAll(StubLinkerInfo *info)
{
	int ret;

	ret = link_functions(info, STUBLINKER_UNLINK, NULL);
	if (ret >= 0)
		ret = link_variables(info, STUBLINKER_UNLINK);
	return ret;
}

/* name unknown */
int StubLinkerUndefAll(StubLinkerInfo *info)
{
	return link_functions(info, STUBLINKER_UNDEF, NULL);
}
