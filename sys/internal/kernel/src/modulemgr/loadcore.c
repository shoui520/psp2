/*
 * loadcore.c - registering exported libraries and linking the imports of
 * modules against them
 */

#include "modulemgr_internal.h"

/* the NIDs of the module's own entry points (NONAME library) */
#define NID_MODULE_START		0x935cd196
#define NID_MODULE_STOP			0x79f8e492
#define NID_MODULE_EXIT			0x913482a9
#define NID_MODULE_BOOTSTART		0x5c424d40
#define NID_MODULE_PROC_PARAM		0x70fba1e7
#define NID_MODULE_START_THREAD_PARAM	0x1a9822a4
#define NID_MODULE_STOP_THREAD_PARAM	0xd20886eb
#define NID_MODULE_SDK_VERSION		0x936c8a78
#define NID_MODULE_PROBES_INFO		0x9318d9dd
#define NID_MODULE_PROBES		0x8ce938b1

/* the import is weak: a missing library is not an error */
#define IMPORT_ATTR_WEAK		0x0008

/* LoadcoreLinkLibraryEntries flags */
#define LINK_FLAG_IGNORE_MISSING	0x04000000
#define LINK_FLAG_NO_UNDEF		0x01000000	/* also: link without patching */
#define UNLINK_FLAG_NO_UNLINK		0x00000010
#define UNLINK_FLAG_FORCE		0x00000020
#define UNLINK_FLAG_SHARED		0x00100000
#define UNLINK_FLAG_UNDEF		0x00200000

/* a library and its client switched to the client's address space */
typedef struct LoadcoreProcessContext {
	void *ctx;
	SceUInt8 saved[0xc];
} LoadcoreProcessContext;

static inline int LoadcoreSwitchProcess(SceUID pid, LoadcoreProcessContext *pc)
{
	int ret;

	if (pid == KERNEL_PID)
		return 0;
	ret = SceSysmemForDriver_2ECF7944(pid, &pc->ctx);
	if (ret < 0)
		return ret;
	return SceSysmemForDriver_2D711589(pc->ctx, pc->saved);
}

static inline void LoadcoreRestoreProcess(SceUID pid, LoadcoreProcessContext *pc)
{
	if (pid != KERNEL_PID)
		SceSysmemForDriver_2D711589(pc->saved, NULL);
}

static inline int LoadcoreLibraryMatches(const LibEntCB *ent, const LibStubCB *stub)
{
	const SceModuleExports *exp = ent->exports;
	const SceModuleImports *imp = stub->imports;

	return exp->libnid == imp->libnid && strncmp(exp->libname, imp->libname, 0x100) == 0 &&
	       imp->version <= exp->version;
}

int LoadcoreUnlinkClient(LibStubCB *stub)
{
	LoadcoreProcessContext pc;
	StubLinkerInfo info;
	int ret;

	ret = LoadcoreSwitchProcess(stub->cb->pid, &pc);
	if (ret < 0)
		return ret;
	ret = LibStubGetNidTable(stub, &info.stub);
	if (ret >= 0) {
		ret = StubLinkerUnlinkAll(&info);
		if (ret < 0)
			SCE_PANIC(258);
	}
	LibStubReleaseNidTable(&info.stub);
	LoadcoreRestoreProcess(stub->cb->pid, &pc);
	return ret;
}

/* name unknown: the NONAME library entries that describe the module itself */
int LoadcoreSetModuleEntry(SceNID nid, void *entry, int type, void *arg)
{
	SceModuleCB *cb = arg;
	SceSize size;
	int ret;

	if (type == 'f') {
		switch (nid) {
		case NID_MODULE_START:
			cb->moduleStart = entry;
			break;
		case NID_MODULE_STOP:
			cb->moduleStop = entry;
			break;
		case NID_MODULE_EXIT:
			cb->moduleExit = entry;
			break;
		case NID_MODULE_BOOTSTART:
			cb->moduleBootstart = entry;
			break;
		}
		return 0;
	}

	switch (nid) {
	case NID_MODULE_PROC_PARAM:
		if (entry != NULL && !ModuleCBIsInRange(cb, (SceUInt32)entry, 1))
			return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
		cb->procParam = entry;
		return 0;
	case NID_MODULE_START_THREAD_PARAM:
		cb->startThreadParam = entry;
		size = sizeof(SceModuleThreadParam);
		break;
	case NID_MODULE_STOP_THREAD_PARAM:
		cb->stopThreadParam = entry;
		size = sizeof(SceModuleThreadParam);
		break;
	case NID_MODULE_SDK_VERSION:
		if (entry == NULL)
			return 0;
		if (!ModuleCBIsInReadOnly(cb, (SceUInt32)entry, 4))
			return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
		if (cb->pid == KERNEL_PID) {
			cb->sdkVersion = *(SceUInt32 *)entry;
			return 0;
		}
		ret = sceKernelCopyFromUserProc(cb->pid, &cb->sdkVersion, entry, 4);
		return ret < 0 ? ret : 0;
	case NID_MODULE_PROBES_INFO:
		if (cb->pid == KERNEL_PID)
			cb->probesInfo = entry;
		size = 1;
		break;
	case NID_MODULE_PROBES:
		if (cb->pid == KERNEL_PID)
			cb->probes = entry;
		size = 1;
		break;
	default:
		return 0;
	}
	if (entry != NULL && !ModuleCBIsInReadOnly(cb, (SceUInt32)entry, size))
		return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
	return 0;
}

void registerClient(LibEntCB *ent, LibStubCB *stub)
{
	LibraryDB *db;
	int intr;

	db = LibraryDBLock(ent->cb->pid, &intr);
	if (db == NULL)
		SCE_PANIC(68);
	stub->linked = ent;
	stub->next = ent->clients;
	ent->clientCount++;
	ent->clients = stub;
	LibraryDBUnlock(db, intr);
}

int LoadcoreLinkClient(LibEntCB *ent, LibStubCB *stub, const ModulemgrLibraryPatch *patch)
{
	LoadcoreProcessContext pc;
	StubLinkerInfo info;
	SceModuleCB *lib = ent->cb, *client = stub->cb;
	SceUID pid;
	int ret;

	if (lib->pid != KERNEL_PID && (client->flags & MODULE_FLAG_SHARED_ANY) != 0 &&
	    (lib->flags & MODULE_FLAG_SHARED_ANY) == 0) {
		SCE_DBG_PRINTF_LEVEL(1, lib->flags & MODULE_FLAG_SHARED_ANY, 174,
			"\nSceModulemgr : cannot export lib to a shared text module\n"
			"   Callee filename : %s\n"
			"          libname  : %s\n"
			"   Caller (shared) : %s\n\n",
			lib->path, ent->exports->libname, client->path);
		return SCE_KERNEL_ERROR_MODULEMGR_SHARED_TEXT;
	}
	pid = client->pid;
#if FW_DEBUG
	if (pid != lib->pid &&
	    (pid == KERNEL_PID || (ent->exports->attr & LIBRARY_ATTR_SYSCALL) == 0)) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 183);
		pid = stub->cb->pid;
	}
#endif
	ret = LoadcoreSwitchProcess(pid, &pc);
	if (ret < 0)
		return ret;
	ret = LibStubGetNidTable(stub, &info.stub);
	if (ret >= 0) {
		ret = LibEntGetNidTable(ent, &info.ent);
		if (ret >= 0) {
			ret = StubLinkerLinkAll(&info, patch);
			if (ret >= 0)
				registerClient(ent, stub);
		}
		LibEntReleaseNidTable(&info.ent);
	}
	LibStubReleaseNidTable(&info.stub);
	LoadcoreRestoreProcess(stub->cb->pid, &pc);
	return ret;
}

/* name unknown: give a kernel library exported to user mode its syscall numbers */
int LoadcoreAssignSyscalls(LibEntCB *ent)
{
	int ret;

	if ((ent->exports->attr & LIBRARY_ATTR_SYSCALL) == 0)
		return 0;
	if (ent->cb->pid != KERNEL_PID)
		return SCE_KERNEL_ERROR_MODULEMGR_INVALID_LIB;
	if (!SyscallCBIsInitialized() || ent->syscallIndex != 0)
		return 0;
	if (ent->exports->nfunc == 0)
		return 0;
	ret = SyscallCBAssignEntry(ent->exports->nfunc);
	if (ret < 0)
		return ret;
	ent->syscallIndex = ret | 0x1000;
	return 0;
}

static inline void LoadcoreRegisterModulesAfterBoot(void)
{
	LibraryDB *db;
	LibEntCB *ent;
	int intr;

	db = LibraryDBLock(KERNEL_PID, &intr);
	if (db == NULL)
		SCE_PANIC(528);
	for (ent = db->libList; ent != NULL; ent = ent->next) {
		if (LoadcoreAssignSyscalls(ent) < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 536, "failed\n");
			SCE_PANIC(537);
		}
	}
	LibraryDBInitializeAddr2ModuleTableForKernel(db);
	LibraryDBUnlock(db, intr);
}

int sceKernelRegisterModulesAfterBoot(void)
{
	int ret;

	ret = SyscallCBInit();
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 551, "can't create the syscall table\n");
		SCE_DBG_ASSERT(0, 552);
	}
	(void)ret;
	LoadcoreRegisterModulesAfterBoot();
	return 0;
}

int LibraryClassCreate(SceLibraryObject *obj)
{
	(void)obj;
	return 0;
}

int LibraryClassDestroy(SceLibraryObject *obj)
{
	obj->libEntCB = NULL;
	return 0;
}

int LoadcoreRegisterLibrary(SceModuleCB *cb, int index, SceUInt32 flags)
{
	LibEntCB *ent = &cb->libEntCB[index];
	SceUID pid = cb->pid;
	LibStubCB *stub, *next;
	int linked, ret, res;

	if (ent->guid != 0)
		return SCE_KERNEL_ERROR_MODULEMGR_OLD_LIB;
	if ((ent->exports->attr & LIBRARY_ATTR_8000) != 0)
		return 0;
	if ((flags & 1) == 0 && (ent->exports->attr & 1) == 0)
		return 0;

	ret = LibraryDBRegisterLib(pid, ent);
	if (ret < 0)
		return ret;
	ret = LoadcoreAssignSyscalls(ent);
	if (ret < 0)
		return ret;

	linked = 0;
	for (stub = LibraryDBGetLostClientHead(pid); stub != NULL; stub = next) {
		next = stub->next;
		if (!LoadcoreLibraryMatches(ent, stub))
			continue;
		LibraryDBReleaseLostClient(stub);
		ret = LoadcoreLinkClient(ent, stub, NULL);
		if (ret >= 0) {
			linked++;
			continue;
		}
		LibraryDBRegisterLostClient(stub);
		/* undo the clients linked so far */
		while (linked != 0) {
			stub = ent->clients;
			if (stub == NULL) {
				SCE_DBG_ASSERT_LEVEL(0, 0, 656);
				return ret;
			}
			ent->clientCount--;
			ent->clients = stub->next;
			stub->next = NULL;
			res = LoadcoreUnlinkClient(stub);
			if (res < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 662,
					"LoadcoreUnlinkClient failed. 0x%08x\n", res);
				SCE_DBG_ASSERT_LEVEL(0, 0, 663);
				return ret;
			}
			LibraryDBRegisterLostClient(stub);
			linked--;
		}
		return ret;
	}
	return 0;
}

int LoadcoreReleaseLibrary(SceModuleCB *cb, int index, SceUInt32 flags)
{
	LibStubCB *stub, *next;
	LibraryDB *db;
	LibEntCB *ent, *lib;
	int intr, ret;

	db = LibraryDBLock(cb->pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NO_LIB;
	ent = &cb->libEntCB[index];
	if (ent->guid == 0) {
		ret = (SceInt16)ent->exports->attr < 0 ? 0 : SCE_KERNEL_ERROR_LIBRARYDB_NO_LIB;
		goto unlock;
	}
	if ((ent->exports->attr & LIBRARY_ATTR_SYSCALL) != 0 && SceSysrootForKernel_5C426B19() > 0xff &&
	    !SceSblACMgrForKernel_BBA13D9C()) {
		ret = SCE_KERNEL_ERROR_MODULEMGR_IN_USE;
		goto unlock;
	}
	stub = ent->clients;
	if (stub != NULL && (flags & UNLINK_FLAG_FORCE) == 0) {
		for (next = stub; next != NULL; next = next->next) {
			if ((next->imports->attr & IMPORT_ATTR_WEAK) == 0) {
				ret = SCE_KERNEL_ERROR_MODULEMGR_IN_USE;
				goto unlock;
			}
		}
	}
	ret = LibraryDBReleaseLib(db, ent);
	if (ret < 0)
		goto unlock;
	LibEntCBClear(ent);
	LibraryDBUnlock(db, intr);

	if (stub == NULL)
		return 0;
	ret = 0;
	next = stub->next;
	for (;;) {
		if ((flags & UNLINK_FLAG_NO_UNLINK) != 0) {
			LibraryDBRegisterLostClient(stub);
		} else {
			ret = LoadcoreUnlinkClient(stub);
			if (ret < 0)
				return ret;
			lib = LibraryDBFindLib(stub);
			if (lib == NULL) {
				LibraryDBRegisterLostClient(stub);
			} else {
				ret = LoadcoreLinkClient(lib, stub, NULL);
				if (ret < 0)
					return ret;
			}
		}
		if (next == NULL)
			return ret;
		stub = next;
		next = next->next;
	}

unlock:
	LibraryDBUnlock(db, intr);
	return ret;
}

/* drop a linked import from the client list of its library */
static inline __attribute__((always_inline)) void releaseClient(LibStubCB *stub, LibEntCB *ent)
{
	LibStubCB *cur, *prev;
	LibraryDB *db;
	int intr, i;

	db = LibraryDBLock(ent->cb->pid, &intr);
	if (db == NULL)
		SCE_PANIC(95);
	cur = ent->clients;
	if (cur == NULL)
		goto notfound;
	if (cur == stub) {
		ent->clients = stub->next;
		if (stub->next == NULL && ent->syscallIndex != 0 && ent->exports->nfunc != 0) {
			for (i = 0; i < ent->exports->nfunc; i++)
				SyscallCBReleaseEntry((ent->syscallIndex + i) & 0xfff);
		}
	} else {
		do {
			prev = cur;
			cur = prev->next;
			if (cur == NULL)
				goto notfound;
		} while (cur != stub);
		prev->next = stub->next;
	}
	stub->linked = NULL;
	ent->clientCount--;
	goto unlock;

notfound:
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 125, "not found a libstubcb\n");
	SCE_DBG_ASSERT_LEVEL(0, 0, 126);
unlock:
	LibraryDBUnlock(db, intr);
}

static inline __attribute__((always_inline)) int
LoadcoreUnlinkLibraryEntries(SceModuleCB *cb, LibStubCB *stub, SceUInt32 flags)
{
	LoadcoreProcessContext pc;
	StubLinkerInfo info;
	LibEntCB *ent;
	int ret;

	if (stub->guid == 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 425, "module:%s stub #%d is free (ignored)\n",
				     cb->name, (int)(stub - cb->libStubCB));
		return 0;
	}
	ent = stub->linked;
	if ((flags & UNLINK_FLAG_SHARED) == 0) {
		if (ent == NULL) {
			LibraryDBReleaseLostClient(stub);
		} else {
			if ((flags & UNLINK_FLAG_NO_UNLINK) == 0) {
				ret = LoadcoreUnlinkClient(stub);
				if (ret < 0)
					return ret;
			}
			releaseClient(stub, ent);
		}
		LibStubCBClear(stub);
		return 0;
	}

	/* a shared module: undo only the links into modules that are not shared */
	if (ent == NULL || (stub->cb->flags & MODULE_FLAG_SHARED_ANY & ~MODULE_FLAG_SHARED_TOOL) != 0)
		return 0;
	if (((ent->exports->attr & LIBRARY_ATTR_SYSCALL) == 0 || (ent->exports->auxattr & 1) != 0) &&
	    (ent->cb->flags & (MODULE_FLAG_SHARED_COPY | MODULE_FLAG_SHARED_ORIGINAL)) == 0)
		return 0;
	if ((flags & UNLINK_FLAG_UNDEF) == 0)
		return LoadcoreUnlinkClient(stub);

	ret = LoadcoreSwitchProcess(stub->cb->pid, &pc);
	if (ret < 0)
		return ret;
	ret = LibStubGetNidTable(stub, &info.stub);
	if (ret >= 0)
		ret = StubLinkerUndefAll(&info);
	LibStubReleaseNidTable(&info.stub);
	LoadcoreRestoreProcess(stub->cb->pid, &pc);
	return ret;
}

int LoadcoreUnlinkLibraries(SceModuleCB *cb, SceUInt32 flags)
{
	int i, ret;

	ret = 0;
	for (i = 0; i < cb->libStubCount; i++) {
		ret = LoadcoreUnlinkLibraryEntries(cb, &cb->libStubCB[i], flags);
		if (ret < 0) {
			SCE_DBG_ASSERT_LEVEL(0, 0, 866);
			continue;
		}
		ret = 0;
	}
	return ret;
}

/* a weak import whose library is missing: point its stubs at the "no entry" code */
static inline __attribute__((always_inline)) void LoadcoreUndefClient(LibStubCB *stub)
{
	LoadcoreProcessContext pc;
	StubLinkerInfo info;
	int ret;

	if (LoadcoreSwitchProcess(stub->cb->pid, &pc) < 0)
		return;
	ret = LibStubGetNidTable(stub, &info.stub);
	if (ret >= 0) {
		ret = StubLinkerUnlinkAll(&info);
		if (ret < 0)
			SCE_PANIC(297);
	}
	LibStubReleaseNidTable(&info.stub);
	LoadcoreRestoreProcess(stub->cb->pid, &pc);
}

static inline __attribute__((always_inline)) const ModulemgrLibraryPatch *
LoadcoreFindPatch(const ModulemgrTitlePatch *patch, const char *libname)
{
	SceUInt32 i;

	if (patch == NULL || patch->count == 0)
		return NULL;
	for (i = 0; i < patch->count; i++) {
		if (strncmp(libname, patch->library[i].libname, 0x100) == 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 394, "try patch library (%s)\n", libname);
			return &patch->library[i];
		}
	}
	return NULL;
}

int LoadcoreLinkLibraryEntries(SceModuleCB *cb, SceUInt32 flags, const ModulemgrTitlePatch *patch)
{
	SceModuleImports *imp;
	LibStubCB *stub;
	LibEntCB *ent;
	int i, ret;

	for (i = 0; i < cb->libStubCount; i++) {
		stub = &cb->libStubCB[i];
		if ((cb->flags & MODULE_FLAG_PROCESS_IMAGE) != 0)
			stub->imports->attr |= IMPORT_ATTR_WEAK;
		ret = LibStubCBCreateUID(stub, i);
		if (ret < 0)
			goto fail;

		ent = LibraryDBFindLib(stub);
		if (ent == NULL) {
			imp = stub->imports;
			if ((imp->attr & IMPORT_ATTR_WEAK) == 0)
				SCE_PRINTF_WITH_INFO(0, 371, "# Library not found: [%s, ver=%d]\n",
						     imp->libname, imp->version);
			if (cb->pid != KERNEL_PID &&
			    strncmp(stub->imports->libname, "SceDTrace", 0x100) == 0)
				goto lost;
			goto missing;
		}
		if ((flags & LINK_FLAG_NO_UNDEF) != 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 383,
				"LoadcoreLinkLibraryEntries : skip LinkClient\n");
			registerClient(ent, stub);
			continue;
		}
		ret = LoadcoreLinkClient(ent, stub, LoadcoreFindPatch(patch, stub->imports->libname));
		if (ret >= 0)
			continue;

		SCE_DBG_PRINTF_LEVEL(1, 0xb, 405, "link client failed (0x%08x)\n", ret);
		imp = stub->imports;
		if (cb->pid != KERNEL_PID) {
			if (strncmp(imp->libname, "SceDTrace", 0x100) == 0)
				goto lost;
			if (ret == SCE_KERNEL_ERROR_MODULEMGR_NO_LIB)
				goto missing;
		} else if (ret == SCE_KERNEL_ERROR_MODULEMGR_NO_LIB) {
missing:
			imp = stub->imports;
			if ((imp->attr & IMPORT_ATTR_WEAK) != 0) {
				LibraryDBRegisterLostClient(stub);
				if ((flags & LINK_FLAG_NO_UNDEF) != 0)
					SCE_DBG_PRINTF_LEVEL(2, 6, 825, "skip undef (lib=%s)\n",
							     stub->imports->libname);
				else
					LoadcoreUndefClient(stub);
				continue;
			}
			if ((flags & LINK_FLAG_IGNORE_MISSING) != 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 834,
					"not found %s (ver:%d), but ignored.\n",
					imp->libname, imp->version);
				goto lost;
			}
			ret = SCE_KERNEL_ERROR_MODULEMGR_NO_LIB;
		}
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 839, "not found %s (ver:%d)\n", imp->libname,
				     imp->version);
		LibStubCBClear(stub);
		goto fail;
lost:
		LibraryDBRegisterLostClient(stub);
	}
	return 0;

fail:
	LoadcoreUnlinkLibraries(cb, 0);
	return ret;
}

/* name unknown: whether a library of the process is still missing */
int LoadcoreIsLibraryImported(SceUID pid, const char *name)
{
	LibraryDB *db;
	LibStubCB *stub;
	int intr, ret;

	db = LibraryDBLock(pid, &intr);
	ret = 0;
	for (stub = db->lostClients; stub != NULL; stub = stub->next) {
		if (strncmp(stub->imports->libname, name, 0x100) == 0) {
			ret = 1;
			break;
		}
	}
	LibraryDBUnlock(db, intr);
	return ret;
}

int LoadcoreExportModuleEntries(SceModuleCB *cb)
{
	LibEntCB *ent = cb->libEntCB;
	int i;

	for (i = 0; i < cb->libEntCount; i++, ent++) {
		if ((SceInt16)ent->exports->attr < 0)
			return LibEntForEachEntry(ent, LoadcoreSetModuleEntry, cb);
	}
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 982, "no libent\n");
	return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD_ENTRY;
}

int LoadcoreRegisterModuleLibraries(SceModuleCB *cb)
{
	int i, ret;

	for (i = 0; i < cb->libEntCount; i++) {
		ret = LoadcoreRegisterLibrary(cb, i, 0);
		if (ret < 0)
			return ret;
	}
	return 0;
}

int LoadcoreUnregisterModuleLibraries(SceModuleCB *cb, SceUInt32 flags)
{
	int i, ret;

	for (i = 0; i < cb->libEntCount; i++) {
		ret = LoadcoreReleaseLibrary(cb, i, flags);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static inline __attribute__((always_inline)) int linkLostClientLibrary(SceUID pid, LibEntCB *ent)
{
	LibStubCB *stub, *next;
	int linked, ret, res;

	linked = 0;
	for (stub = LibraryDBGetLostClientHead(pid); stub != NULL; stub = next) {
		next = stub->next;
		if (!LoadcoreLibraryMatches(ent, stub))
			continue;
		LibraryDBReleaseLostClient(stub);
		ret = LoadcoreLinkClient(ent, stub, NULL);
		if (ret >= 0) {
			linked++;
			continue;
		}
		LibraryDBRegisterLostClient(stub);
		/* undo the clients linked so far */
		while (linked != 0) {
			stub = ent->clients;
			linked--;
			if (stub == NULL) {
				SCE_DBG_ASSERT_LEVEL(0, 0, 1071);
				return ret;
			}
			ent->clientCount--;
			ent->clients = stub->next;
			stub->next = NULL;
			res = LoadcoreUnlinkClient(stub);
			if (res < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 1077,
					"LoadcoreUnlinkClient failed. 0x%08x\n", res);
				SCE_DBG_ASSERT_LEVEL(0, 0, 1078);
				return ret;
			}
			LibraryDBRegisterLostClient(stub);
		}
		return ret;
	}
	return linked != 0 ? 0 : SCE_KERNEL_ERROR_LIBRARYDB_NO_LIB;
}

int sceKernelLinkWeakLibraryByLibname(SceUID pid, const char *libname)
{
	LibEntCB *ent;
	int ret;

	if (libname == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ent = LibraryDBFindSyscallLib(libname);
	if (ent == NULL)
		ret = SCE_KERNEL_ERROR_MODULEMGR_NO_LIB;
	else
		ret = linkLostClientLibrary(pid, ent);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1101, "linkLostClientLibrary : failed (res:0x%08x)\n",
				     ret);
	SceCpuForKernel_803C84BF();
	return ret;
}
