/*
 * SceNidsymtblForDriver: names of the kernel's exported functions by NID,
 * sorted by NID. Names without SCE_NIDSYMTBL_PUBLIC are given out only while
 * system application debugging is allowed.
 */

#include <acmgr/acmgr.h>
#include <kernel/error.h>
#include <syslibtrace/syslibtrace.h>
#include "syslibtrace.h"

#define NIDSYM(_flags, _nid, _name)	{ SCE_NIDSYMTBL_##_flags, (_nid), (_name) }
#define SCE_NIDSYMTBL_DEBUG		0

static const SceNidsymtblEntry s_nidsymtbl[] = {
#include "nidsymtbl.inc"
};

#define NIDSYMTBL_NUM	(sizeof(s_nidsymtbl) / sizeof(s_nidsymtbl[0]))

/* name unknown: bsearch order of two entries */
static __attribute__((noinline, noclone)) int NidsymtblCompare(const SceNidsymtblEntry *a,
						      const SceNidsymtblEntry *b)
{
	if (a->nid < b->nid)
		return -1;
	if (a->nid > b->nid)
		return 1;
	return 0;
}

/* name unknown: the whole table, returns its row count */
SceSize SceNidsymtblForDriver_138A9106(const SceNidsymtblEntry **table)
{
	*table = s_nidsymtbl;
	return NIDSYMTBL_NUM;
}

/* name unknown: the name of a NID */
int SceNidsymtblForDriver_985E2935(SceUInt32 nid, const char **name)
{
	SceNidsymtblEntry key = { 0, nid, "" };
	const SceNidsymtblEntry *entry;
	SceUInt32 lo = 0, hi = NIDSYMTBL_NUM - 1, mid;
	int cmp;

	/* unsigned bounds: a NID below the first row walks off the table (as Sony's does) */
	for (;;) {
		if (lo > hi)
			return SCE_ERROR_ERRNO_ENOENT;
		mid = (lo + hi) / 2;
		entry = &s_nidsymtbl[mid];
		cmp = NidsymtblCompare(entry, &key);
		if (cmp == 0)
			break;
		if (cmp > 0)
			hi = mid - 1;
		else
			lo = mid + 1;
	}
	if (entry == NULL)
		return SCE_ERROR_ERRNO_ENOENT;

	if (!ksceSblQafMgrIsAllowSystemAppDebug() && !(entry->flags & SCE_NIDSYMTBL_PUBLIC))
		return SCE_ERROR_ERRNO_EPERM;
	if (name == NULL)
		return 0;
	*name = entry->name;
	return 0;
}
