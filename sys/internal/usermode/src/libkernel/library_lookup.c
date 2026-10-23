#include <modulemgr/module_library.h>
#include <kernel/error.h>
#include <libkernel/libkernel.h>

extern int sceKernelGetLibraryInfoByNID(SceUID moduleId, SceNID libraryNid,
		SceKernelLibraryInfo *info);

/* Resolve function entries in request order, retaining completed output on error. */
int SceLibKernel_BEF71602(SceUID moduleId, SceNID libraryNid,
		unsigned int count, const SceNID *nids, void **entries)
{
	SceKernelLibraryInfo info = { .size = sizeof(info) };
	unsigned int requested, index;
	SceNID requestedNid;
	int result;
	void *entry;

	result = sceKernelGetLibraryInfoByNID(moduleId, libraryNid, &info);
	if (result < 0)
		return result;
	if (!count)
		return 0;
	requestedNid = nids[0];
	for (requested = 0; ; ) {
		for (index = 0; index < info.nfunc; index++) {
			if (requestedNid == info.nidtable[index])
				break;
		}
		if (index == info.nfunc)
			return SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID;
		entry = info.entrytable[index];
		if (!entry)
			return SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID;
		entries[requested++] = entry;
		if (requested == count)
			return 0;
		requestedNid = nids[requested];
	}
}

/* Name and the meanings of the two unused arguments are unknown. */
int SceLibKernel_70867F93(SceUInt unused0, SceUInt unused1, SceSize count,
		SceUInt32 *words)
{
	(void)unused0;
	(void)unused1;
	if (count)
		sceClibMemset(words, 0, count << 2);
	return 0;
}

/* Variables follow the function entries in both parallel export tables. */
int SceLibKernel_0A4DF821(SceUID moduleId, SceNID libraryNid,
		SceNID variableNid, void **entry)
{
	SceKernelLibraryInfo info = { .size = sizeof(info) };
	unsigned int index;
	void *address;
	int result;

	result = sceKernelGetLibraryInfoByNID(moduleId, libraryNid, &info);
	if (result < 0)
		return result;
	for (index = 0; index < info.nvar; index++) {
		if (variableNid == info.nidtable[info.nfunc + index])
			break;
	}
	if (index == info.nvar)
		return SCE_KERNEL_ERROR_MODULEMGR_NO_VAR_NID;
	address = info.entrytable[info.nfunc + index];
	if (!address)
		return SCE_KERNEL_ERROR_MODULEMGR_NO_VAR_NID;
	*entry = address;
	return 0;
}
