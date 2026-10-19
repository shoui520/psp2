/*
 * exidx.c - finding the ARM exception index entry of an address
 * (Sony's file name is unknown; the code sits between syscallcb.c and
 * backtrace.c and is built like the rest of the module, not with -Os)
 */

#include "modulemgr_internal.h"

#define EXIDX_CANTUNWIND	1

/* an entry of .ARM.exidx */
typedef struct ExidxEntry {
	SceUInt32 fn;			/* prel31 offset of the function */
	SceUInt32 insn;			/* inline unwind data, prel31 offset or EXIDX_CANTUNWIND */
} ExidxEntry;

static inline SceUInt32 prel31(SceUInt32 v)
{
	if ((v & 0x40000000) != 0)
		v |= 0x80000000;
	return v;
}

/* name unknown: out[0] the second word of the entry, out[1] its unwind table or -1 */
int BacktraceFindExidx(SceUID pid, SceUInt32 addr, SceUInt32 *out, SceUInt32 flags)
{
	const ExidxEntry *copy, *e;
	SceUInt32 top, btm, n, lo, hi, mid, next, fn, fnNext, base, baseNext, w;
	ExidxEntry entry;
	SceModuleCB *cb;
	LibraryDB *db;
	int intr, ret;

	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	ret = LibraryDBFindModuleByAddr(db, addr, &cb);
	if (ret < 0)
		goto out;

	copy = cb->exidxCopy;
	if ((flags & 8) != 0 || copy == NULL) {
		/* walk the module's own table from the end */
		top = (SceUInt32)cb->exidxTop;
		btm = (SceUInt32)cb->exidxBtm;
		if (top < btm && ((btm - top) & 7) == 0) {
			do {
				btm -= 8;
				if (ModulemgrCopyIn(pid, &entry, (const void *)btm, 8) < 0)
					break;
				entry.fn = prel31(entry.fn);
				if (((entry.fn + btm) & ~1) <= (addr & ~1)) {
					out[0] = entry.insn;
					if ((SceInt32)entry.insn < 0 || entry.insn == EXIDX_CANTUNWIND) {
						out[1] = -1;
					} else {
						out[1] = btm + entry.insn + 4;
					}
					ret = 0;
					goto out;
				}
			} while (top != btm);
		}
		ret = SCE_KERNEL_ERROR_MODULEMGR_NOENT;
		goto out;
	}

	/* binary search in the copy of the table */
	top = (SceUInt32)cb->exidxTop;
	btm = (SceUInt32)cb->exidxBtm;
	if (top >= btm || ((btm - top) & 7) != 0) {
		ret = SCE_KERNEL_ERROR_MODULEMGR_NOENT;
		goto out;
	}
	n = (btm - top) >> 3;
	addr &= ~1;
	hi = n - 1;
	e = &copy[hi];
	base = pid == SCE_KERNEL_PROCESS_ID ? (SceUInt32)e : top + hi * 8;
	if (addr < ((prel31(e->fn) + base) & ~1)) {
		lo = 0;
		mid = hi >> 1;
		if (hi <= mid) {
			ret = SCE_KERNEL_ERROR_MODULEMGR_NOENT;
			goto out;
		}
		for (;;) {
			e = &copy[mid];
			fn = prel31(e[0].fn);
			fnNext = prel31(e[1].fn);
			if (pid == SCE_KERNEL_PROCESS_ID) {
				base = (SceUInt32)&e[0];
				baseNext = (SceUInt32)&e[1];
			} else {
				base = top + mid * 8;
				baseNext = top + (mid + 1) * 8;
			}
			if (((fn + base) & ~1) <= addr) {
				if (addr < ((fnNext + baseNext) & ~1))
					break;
				lo = mid;
			} else {
				hi = mid;
			}
			next = (lo + hi) >> 1;
			if (mid == next || next >= hi) {
				ret = SCE_KERNEL_ERROR_MODULEMGR_NOENT;
				goto out;
			}
			mid = next;
		}
	}

	w = e->insn;
	out[0] = w;
	if ((SceInt32)w < 0 || w == EXIDX_CANTUNWIND) {
		out[1] = -1;
	} else if (pid == SCE_KERNEL_PROCESS_ID) {
		out[1] = prel31(w) + (SceUInt32)&e->insn;
	} else {
		out[1] = prel31(w) + top + ((SceUInt32)&e->insn - (SceUInt32)copy);
	}
	ret = 0;
out:
	LibraryDBUnlock(db, intr);
	return ret;
}
