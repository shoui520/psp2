#include "../../../../../../common/module_library.h"
#include "sce/error.h"

extern int sceKernelGetLibraryInfoByNID(SceUID moduleId, SceNID libraryNid,
		SceKernelLibraryInfo *info);

/* Resolve function entries in request order, retaining completed output on error. */
int SceLibKernel_BEF71602(SceUID moduleId, SceNID libraryNid,
		unsigned int count, const SceNID *nids, void **entries)
{
	SceKernelLibraryInfo info = { .size = sizeof(info) };
	unsigned int requested, index;
	SceNID requestedNid, firstNid;
	int result;
	void *entry;

	result = sceKernelGetLibraryInfoByNID(moduleId, libraryNid, &info);
	if (result < 0)
		return result;
	if (!count)
		return 0;
	requestedNid = *(const volatile SceNID *)nids;
	if (!info.nfunc)
		return SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID;
	firstNid = info.nidtable[0];
	for (requested = 0; ; requested++) {
		for (index = 0; ; index++) {
			if (requestedNid == (index ? info.nidtable[index] : firstNid))
				break;
			if (index + 1 == info.nfunc)
				return SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID;
		}
		entry = info.entrytable[index];
		if (!entry)
			return SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID;
		entries[requested] = entry;
		if (requested + 1 == count)
			break;
		requestedNid = nids[requested + 1];
	}
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
