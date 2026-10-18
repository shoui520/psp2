#include "exidx.h"
#include "lifecycle.h"
#include <scetypes.h>
#include <kernel/error.h>

/* the exception index of each user page in [USER_BASE, USER_END), kept by the kernel */
#define EXIDX_PAGES		((const struct ExidxPage *)0xee000000)
#define USER_BASE		0x81000000
#define USER_END		0xf0000000

struct ExidxPage {
	const struct ExidxEntry *begin;
	const struct ExidxEntry *end;
};

/* The process argument is unused in the user-mode implementation. */
int BacktraceSearchExidx(SceUID processId, uintptr_t address,
		const struct ExidxEntry *entries, uintptr_t begin, uintptr_t end,
		struct ExidxResult *result)
{
	const struct ExidxEntry *entry;
	uint32_t descriptor;

	if (begin >= end)
		return SCE_KERNEL_ERROR_MODULEMGR_NOENT;
	entry = ExidxFindEntry(entries, (uintptr_t)entries, (end - begin) >> 3,
			      address);
	if (!entry)
		return SCE_KERNEL_ERROR_MODULEMGR_NOENT;
	descriptor = entry->unwind;
	result->descriptor = descriptor;
	if ((int32_t)descriptor < 0 || descriptor == 1)
		result->instructions = (const uint32_t *)UINTPTR_MAX;
	else
		result->instructions = (const uint32_t *)
			((uintptr_t)&entry->unwind + ExidxPrel31(descriptor));
	return 0;
}

int BacktraceFindExidx(SceUID processId, uintptr_t address,
		struct ExidxResult *result)
{
	const struct ExidxPage *pages = EXIDX_PAGES;
	const struct ExidxEntry *begin, *end;
	uintptr_t offset = address - USER_BASE;

	if (offset >= USER_END - USER_BASE)
		return SCE_KERNEL_ERROR_MODULEMGR_NOENT;
	begin = pages[offset >> 12].begin;
	end = pages[offset >> 12].end;
	return BacktraceSearchExidx(0, address, begin, (uintptr_t)begin,
				   (uintptr_t)end, result);
}

int SceLibKernel_6C7365C4(uintptr_t address, struct ExidxResult *result)
{
	const struct ExidxPage *pages = EXIDX_PAGES;
	const struct ExidxEntry *begin, *end;
	uintptr_t offset = address - USER_BASE;

	if (!libkernelState.backtraceAvailable || offset >= USER_END - USER_BASE)
		return SCE_KERNEL_ERROR_ILLEGAL_PERMISSION;
	begin = pages[offset >> 12].begin;
	end = pages[offset >> 12].end;
	return BacktraceSearchExidx(0, address, begin, (uintptr_t)begin,
				   (uintptr_t)end, result);
}
