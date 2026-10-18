/*
 * librarydb.c - the per-process library database (LibraryDB): the module
 * list, the exported libraries, the clients whose library is missing and,
 * in tool mode, the page -> module tables used for backtraces.
 */

#include "modulemgr_internal.h"
#include <modulemgr/modulemgr_debugger.h>

#define PROCESS_TYPE_SHARED	0x4000000	/* processes that share module images */

int g_sharedFlag;			/* a shared-image process exists */
SceKernelSpinlock g_sharedLock;
SharedModuleEntry *g_sharedModules;
Addr2ModTable g_kernelAddr2Mod;

/* the debug build keeps it out of line (its assertion reports the caller) */
#if FW_DEBUG
static __attribute__((noinline)) LibraryDB *LibraryDBGetDB(SceUID pid)
#else
static LibraryDB *LibraryDBGetDB(SceUID pid)
#endif
{
	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 67);
	return sceKernelGetProcessLibdb(pid);
}

/* name unknown: the database of a process, locked */
LibraryDB *LibraryDBLock(SceUID pid, int *intr)
{
	LibraryDB *db;

	if (ModulemgrReferProcess(pid) < 0)
		return NULL;
	db = LibraryDBGetDB(pid);
	if (db != NULL)
		*intr = SceCpuForDriver_D32ACE9E(&db->lock);
	ModulemgrReleaseProcess(pid);
	return db;
}

/* name unknown: unlink the newest library with this NID and name */
LibEntCB *LibraryDBUnlinkLib(LibraryDB *db, SceNID libnid, const char *name)
{
	LibEntCB *lib, *prev;

	if (db->libList == NULL)
		return NULL;
	prev = NULL;
	lib = db->libList;
	do {
		if (lib->exports->libnid == libnid &&
		    strncmp(lib->exports->libname, name, 0x100) == 0) {
			if (prev == NULL)
				db->libList = lib->next;
			else
				prev->next = lib->next;
			return lib;
		}
		prev = lib;
		lib = lib->next;
	} while (lib != NULL);
	return NULL;
}

/* name unknown: the table slot of the page at addr */
SceModuleCB **Addr2ModGetEntry(SceModuleCB **table, Addr2ModExidx **exidxBase, SceUInt32 addr,
			       Addr2ModExidx **exidx)
{
	SceUInt32 page;

	*exidx = NULL;
	page = (addr - 0x200000) >> 12;
	if (page >= 0x8000) {
		page = (addr - 0x8000000) >> 12;
		if (page >= 0x8000) {
			if (addr - 0x81000000 >= 0x6f000000)
				return NULL;
			page = (addr - 0x81000000) >> 12;
			*exidx = *exidxBase + page;
			return table + page;
		}
		page += 0x8000;
	}
	return table + page;
}

/* name unknown: enter the pages of the first segment of a module */
void Addr2ModRegister(Addr2ModTable *tbl, SceModuleCB *cb)
{
	SceUInt32 vaddr = (SceUInt32)cb->segment[0].vaddr;
	SceSize memsz = cb->segment[0].memsz;
	Addr2ModExidx *exidx;
	SceModuleCB **ent;
	SceUInt32 offset, end;

	ent = Addr2ModGetEntry(tbl->table, &tbl->exidx, vaddr, &exidx);
	if (ent == NULL)
		return;
	offset = vaddr & 0xfff;
	end = memsz + offset;
	for (; offset < end; offset += 0x1000) {
		*ent++ = cb;
		if (exidx != NULL) {
			exidx->top = cb->exidxTop;
			exidx->btm = cb->exidxBtm;
			exidx++;
		}
	}
}

/* name unknown: user libraries get process-local UIDs from SDK 0x01800000 on */
int LibraryDBUsesPUID(SceUID pid)
{
	SceUInt32 sdkVersion = 0;

	if (SceProcessmgrForDriver_D141C076(pid, &sdkVersion) < 0)
		return 0;
	if (sdkVersion < 0x1800000)
		return 0;
	return 1;
}

static SceUID GetLibGUIDByLibUID(SceUID pid, SceUID uid)
{
	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 149);
	if (pid != KERNEL_PID && LibraryDBUsesPUID(pid) == 1)
		return SceSysmemForDriver_45D22597(pid, uid);
	return uid;
}

/* name unknown: refer a library object and its module */
SceLibraryObject *LibraryDBReferLibrary(SceUID pid, SceUID uid)
{
	SceLibraryObject *obj;
	SceUID guid;

	guid = GetLibGUIDByLibUID(pid, uid);
	if (guid < 0 || SceSysmemForDriver_00ED6C14(guid, g_libraryClass, (SceObjectBase **)&obj) < 0)
		return NULL;
	if (ModulemgrReferModule(obj->modid) == NULL) {
		sceGUIDReleaseObject(guid);
		return NULL;
	}
	return obj;
}

static SceUID GetLibUIDByLibEntCB(SceUID pid, const SceUID *guid, const SceUID *puid)
{
	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 190);
	if (pid != KERNEL_PID && LibraryDBUsesPUID(pid) == 1)
		return *puid;
	return *guid;
}

/* name unknown */
void LibraryDBUnlock(LibraryDB *db, int intr)
{
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
}

void LibraryDBDelete(SceUID pid)
{
	LibraryDB *db, old;
	LibStubCB *stub;
	Addr2ModTable *tbl;
	int intr;

	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return;
	old = *db;
	memset(db, 0, offsetof(LibraryDB, lock));
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);

	stub = old.lostClients;
	while (stub != NULL) {
		LibStubCB *next = stub->next;

		old.lostCount--;
		LibStubCBClear(stub);
		stub = next;
	}
	tbl = old.addr2mod;
	if (tbl != NULL) {
		SceSysmemForDriver_009E1C61(tbl->tableBlock);
		if (tbl->exidxKernelBlock > 0)
			SceSysmemForDriver_009E1C61(tbl->exidxKernelBlock);
		if (tbl->exidxBlock > 0)
			SceSysmemForDriver_009E1C61(tbl->exidxBlock);
		sceKernelFreeRemoteToolHeap(pid, tbl);
	}
	SCE_DBG_ASSERT_LEVEL(0, old.libCount == 0, 273);
	SCE_DBG_ASSERT_LEVEL(0, old.lostCount == 0, 274);
	SCE_DBG_ASSERT_LEVEL(0, old.moduleCount == 0, 275);
}

/* name unknown: create the database of a new process */
LibraryDB *LibraryDBInitializeAddr2ModuleTable(SceUID pid)
{
	SceKernelAllocMemBlockKernelOpt opt;
	LibraryDB *db;
	Addr2ModTable *tbl;
	char titleid[0x20];

	db = ModulemgrAlloc(pid, sizeof(*db));
	if (db == NULL)
		return NULL;
	memset(db, 0, sizeof(*db));
	db->pid = pid;

	if (SceDipswForDriver_A98FC2FD(0xd2)) {
		SCE_DBG_ASSERT(pid != KERNEL_PID, 405);
		tbl = sceKernelAllocRemoteToolHeap(pid, sizeof(*tbl));
		if (tbl != NULL) {
			memset(tbl, 0, sizeof(*tbl));
			db->addr2mod = tbl;

			memset(&opt, 0, sizeof(opt));
			opt.size = sizeof(opt);
			opt.attr = 0xd0000000;
			tbl->tableBlock = SceSysmemForDriver_C94850C9("SceModuleMgrAddr2ModTbl",
								  0x10f0d006, 0x1bc000, &opt);
			if (tbl->tableBlock < 0) {
				sceKernelFreeRemoteToolHeap(pid, tbl);
				db->addr2mod = NULL;
				goto fail;
			}
			SceSysmemForDriver_A841EDDA(tbl->tableBlock, &tbl->table);

			memset(&opt, 0, sizeof(opt));
			opt.size = sizeof(opt);
			opt.attr = 0xe0080081;
			opt.vbase = 0xee000000;
			opt.pid = pid;
			tbl->exidxBlock = SceSysmemForDriver_C94850C9("SceModuleMgrA2ExidxTmp",
								  0x3f0d040, 0x378000, &opt);
			if (tbl->exidxBlock < 0)
				goto fail;

			memset(&opt, 0, sizeof(opt));
			opt.size = sizeof(opt);
			opt.attr = 0xe1000040;
			opt.mirror_blockid = tbl->exidxBlock;
			tbl->exidxKernelBlock = SceSysmemForDriver_C94850C9("SceModuleMgrA2ExidxTmp",
									0x10f0d006, 0, &opt);
			if (tbl->exidxKernelBlock < 0)
				goto fail;
			SceSysmemForDriver_A841EDDA(tbl->exidxKernelBlock, &tbl->exidx);
			if (SceSysmemForDriver_58D21746(tbl->exidxBlock) < 0)
				goto fail;
		}
	}

	if (g_sharedFlag)
		db->flags |= LIBDB_FLAG_SHARED;
	if (SceSysrootForKernel_EC3124A3(pid, titleid, sizeof(titleid)) >= 0 &&
	    strncmp(titleid, "PCSG00307", sizeof(titleid)) == 0)
		db->flags |= LIBDB_FLAG_PCSG00307;
	return db;

fail:
	LibraryDBDelete(pid);
	return NULL;
}

int LibraryDBInitializeAddr2ModuleTableForKernel(LibraryDB *db)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SceModuleCB *cb;

	if (SceDipswForDriver_A98FC2FD(0xd2)) {
		db->addr2mod = &g_kernelAddr2Mod;
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = 0xd0000000;
		g_kernelAddr2Mod.tableBlock = SceSysmemForDriver_C94850C9("SceKernelProcess", 0x10f0d006,
								      0x40000, &opt);
		if (g_kernelAddr2Mod.tableBlock < 0) {
			SCE_DBG_ASSERT_LEVEL(0, 0, 384);
			return g_kernelAddr2Mod.tableBlock;
		}
		SceSysmemForDriver_A841EDDA(g_kernelAddr2Mod.tableBlock, &g_kernelAddr2Mod.table);
		for (cb = db->modules; cb != NULL; cb = cb->next)
			Addr2ModRegister(&g_kernelAddr2Mod, cb);
	}
	return 0;
}

/* name unknown: refer the original of a shared module by path */
SharedModuleEntry *SharedModuleFind(const char *path)
{
	SharedModuleEntry *ent;
	int intr;

	intr = SceCpuForDriver_D32ACE9E(&g_sharedLock);
	for (ent = g_sharedModules; ent != NULL; ent = ent->next) {
		if (strncmp(ent->cb->path, path, 0x100) == 0) {
			ent->refCount++;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&g_sharedLock, intr);
			return ent;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_sharedLock, intr);
	return NULL;
}

/* name unknown */
int SharedModuleRelease(SharedModuleEntry *ent)
{
	int intr;

	intr = SceCpuForDriver_D32ACE9E(&g_sharedLock);
	ent->refCount--;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_sharedLock, intr);
	return 0;
}

static inline void SharedLibraryDBReleaseModule(SceModuleCB *cb)
{
	SharedModuleEntry *ent, *prev;
	int intr;

	intr = SceCpuForDriver_D32ACE9E(&g_sharedLock);
	ent = g_sharedModules;
	if (ent == NULL)
		goto unlock;
	if (ent->cb == cb) {
		g_sharedModules = ent->next;
	} else {
		do {
			prev = ent;
			ent = prev->next;
			if (ent == NULL)
				goto unlock;
		} while (ent->cb != cb);
		prev->next = ent->next;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_sharedLock, intr);
	if (ent->refCount != 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 565, "shared module with client is killed!?\n");
		SCE_PANIC(566);
	}
	if (ent->dataCopy != NULL)
		SceSysmemForKernel_4233C16D(ent->dataCopy);
	SceSysmemForKernel_4233C16D(ent);
	return;

unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_sharedLock, intr);
}

void LibraryDBReleaseModule(SceModuleCB *cb)
{
	Addr2ModExidx *exidx;
	SceModuleCB **ent, *cur, *prev;
	Addr2ModTable *tbl;
	LibraryDB *db;
	SceUInt32 vaddr, pages;
	int intr;

	db = LibraryDBLock(cb->pid, &intr);
	if (db == NULL) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 653);
		return;
	}

	if (cb->pid != KERNEL_PID) {
		if ((cb->flags & MODULE_FLAG_SHARED_MASK) != 0) {
			SharedModuleRelease(cb->shared);
		} else if ((cb->flags & MODULE_FLAG_SHARED_ORIGINAL) != 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 666, "An original shared library is unloaded : %s\n",
					     cb->name);
			SharedLibraryDBReleaseModule(cb);
		}
	}

	cur = db->modules;
	if (cur == NULL)
		goto notfound;
	if (cur == cb) {
		db->modules = cb->next;
	} else {
		do {
			prev = cur;
			cur = prev->next;
			if (cur == NULL)
				goto notfound;
		} while (cur != cb);
		prev->next = cb->next;
	}
	tbl = db->addr2mod;
	db->moduleCount--;
	cb->next = NULL;
	if (tbl != NULL) {
		vaddr = (SceUInt32)cb->segment[0].vaddr;
		pages = cb->segment[0].memsz;
		ent = Addr2ModGetEntry(tbl->table, &tbl->exidx, vaddr, &exidx);
		if (ent != NULL) {
			pages = ((vaddr & 0xfff) + pages + 0xfff) >> 12;
			memset(ent, 0, pages * sizeof(*ent));
			if (exidx != NULL)
				memset(exidx, 0, pages * sizeof(*exidx));
		}
	}
	goto unlock;

notfound:
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 697, "not found a module\n");
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
}

void LibraryDBRegisterLostClient(LibStubCB *stub)
{
	LibraryDB *db;
	int intr;

	db = LibraryDBLock(stub->cb->pid, &intr);
	if (db == NULL) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 710);
		return;
	}
	stub->linked = NULL;
	stub->next = db->lostClients;
	db->lostCount++;
	db->lostClients = stub;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
}

void LibraryDBReleaseLostClient(LibStubCB *stub)
{
	LibStubCB *cur, *prev;
	LibraryDB *db;
	int intr;

	db = LibraryDBLock(stub->cb->pid, &intr);
	if (db == NULL) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 732);
		return;
	}
	cur = db->lostClients;
	if (cur == NULL)
		goto unlock;
	if (cur == stub) {
		db->lostClients = stub->next;
	} else {
		do {
			prev = cur;
			cur = prev->next;
			if (cur == NULL)
				goto unlock;
		} while (cur != stub);
		prev->next = stub->next;
	}
	db->lostCount--;
	stub->next = NULL;
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
}

/* an older version of the library may only be replaced by a newer one of the same kind */
static inline int canRegisterLib(LibraryDB *db, LibEntCB *ent)
{
	SceModuleExports *exports = ent->exports, *cur;
	LibEntCB *lib;

	for (lib = db->libList; lib != NULL; lib = lib->next) {
		if (lib->exports->libnid != exports->libnid ||
		    strncmp(lib->exports->libname, exports->libname, 0x100) != 0)
			continue;
		ent->older = lib;
		cur = lib->exports;
		if ((exports->attr & LIBRARY_ATTR_NOLINK) != 0) {
			if ((cur->attr & LIBRARY_ATTR_NOLINK) != 0)
				return 1;
			SCE_DBG_PRINTF_LEVEL(1, 0, 780,
					     "Can't register '%s' public library version 0x%04X. (current=0x%04X)\n",
					     exports->libname, exports->version, cur->version);
			return 0;
		}
		if ((cur->attr & LIBRARY_ATTR_NOLINK) != 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 787,
					     "Can't register '%s' nolink library version 0x%04X. (current=0x%04X)\n",
					     exports->libname, exports->version, cur->version);
			return 0;
		}
		if (cur->version < exports->version)
			return 1;
		SCE_DBG_PRINTF_LEVEL(1, 0, 793,
				     "Can't register '%s' library version 0x%04X. (current=0x%04X)\n",
				     exports->libname, exports->version, cur->version);
		return 0;
	}
	ent->older = NULL;
	return 1;
}

/* name unknown: create the UIDs of a library and make it the current version */
int LibraryDBRegisterLib(SceUID pid, LibEntCB *ent)
{
	UIDCreateOpt opt, *optp;
	SceLibraryObject *obj;
	LibraryDB *db;
	int usePuid, intr;
	SceUID uid;

	db = LibraryDBLock(pid, &intr);
	if (db == NULL) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 823);
		LibEntCBClear(ent);
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	}
	if (!canRegisterLib(db, ent)) {
		uid = SCE_KERNEL_ERROR_MODULEMGR_OLD_LIB;
		goto clear;
	}

	usePuid = 0;
	optp = NULL;
	if (pid != KERNEL_PID && LibraryDBUsesPUID(pid)) {
		opt.flags = UID_CREATE_OPT_PROCESS_HEAP;
		opt.object = NULL;
		opt.heapGuid = 0;
		opt.pid = pid;
		opt.heapKind = (ent->cb->flags & MODULE_FLAG_TYPE_MASK) == 1;
		opt.type = 0;
		opt.attr = 0;
		opt.ownerPid = 0;
		opt.cls = NULL;
		optp = &opt;
		usePuid = 1;
	}
	uid = sceGUIDKernelCreateWithOpt(g_libraryClass, ent->exports->libname, optp, &obj);
	if (uid < 0)
		goto clear;
	ent->guid = uid;
	obj->libEntCB = ent;
	obj->modid = ent->cb->modid;
	if (usePuid) {
		uid = SceSysmemForDriver_BF209859(pid, uid);
		if (uid < 0) {
			sceGUIDClose(ent->guid);
			ent->guid = 0;
			goto unlock;
		}
		ent->puid = uid;
	}
	if (ent->older != NULL)
		LibraryDBUnlinkLib(db, ent->exports->libnid, ent->exports->libname);
	ent->next = db->libList;
	db->libCount++;
	db->libList = ent;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return 0;

clear:
	LibEntCBClear(ent);
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return uid;
}

/* name unknown: remove a library version; the next older one becomes current */
int LibraryDBReleaseLib(LibraryDB *db, LibEntCB *ent)
{
	LibEntCB *head, *cur, *prev;

	head = LibraryDBUnlinkLib(db, ent->exports->libnid, ent->exports->libname);
	if (head == NULL) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 903, "can't find a library\n");
		return SCE_KERNEL_ERROR_LIBRARYDB_NO_LIB;
	}
	if (head == ent) {
		head = ent->older;
		ent->older = NULL;
	} else {
		for (prev = head; (cur = prev->older) != NULL; prev = cur) {
			if (cur == ent) {
				prev->older = ent->older;
				ent->older = NULL;
				break;
			}
		}
	}
	if (head != NULL) {
		head->next = db->libList;
		db->libList = head;
	}
	db->libCount--;
	return 0;
}

static inline LibEntCB *findLib(SceUID pid, SceNID libnid, const char *name)
{
	LibEntCB *lib;
	LibraryDB *db;
	int intr;

	db = LibraryDBLock(pid, &intr);
	if (db == NULL) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 942);
		return NULL;
	}
	for (lib = db->libList; lib != NULL; lib = lib->next) {
		if (lib->exports->libnid == libnid &&
		    (lib->exports->attr & LIBRARY_ATTR_NOLINK) == 0 &&
		    strncmp(lib->exports->libname, name, 0x100) == 0) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
			return lib;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return NULL;
}

/* name unknown: the library an import resolves to, in the process or the kernel */
LibEntCB *LibraryDBFindLib(LibStubCB *stub)
{
	SceUID pid = stub->cb->pid, target;
	SceUInt16 version;
	SceNID libnid;
	LibEntCB *lib;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 1019);
	version = stub->imports->version;
	libnid = stub->imports->libnid;
	for (target = pid;; target = KERNEL_PID) {
		lib = findLib(target, libnid, stub->imports->libname);
		if (lib != NULL && lib->exports->version >= version &&
		    (pid != KERNEL_PID || (lib->exports->attr & LIBRARY_ATTR_SYSCALL) == 0))
			return lib;
		if (target == KERNEL_PID)
			return NULL;
	}
}

/* name unknown */
LibEntCB *LibraryDBFindSyscallLib(const char *name)
{
	LibEntCB *lib;
	LibraryDB *db;
	int intr;

	db = LibraryDBLock(KERNEL_PID, &intr);
	if (db == NULL) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 1057);
		return NULL;
	}
	for (lib = db->libList; lib != NULL; lib = lib->next) {
		if ((lib->exports->attr & (LIBRARY_ATTR_SYSCALL | LIBRARY_ATTR_NOLINK)) ==
		    LIBRARY_ATTR_SYSCALL &&
		    strncmp(lib->exports->libname, name, 0x100) == 0)
			break;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return lib;
}

int LibraryDBGetLibraryInfoByNID(SceUID pid, SceUID modid, SceNID libnid,
				 SceKernelLibraryInfo *info)
{
	SceModuleExports *exports;
	LibEntCB *lib, *ver;
	LibraryDB *db;
	int ret, intr;

	db = LibraryDBLock(pid, &intr);
	if (db == NULL) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 1087);
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	}
	for (lib = db->libList; lib != NULL; lib = lib->next) {
		for (ver = lib; ver != NULL; ver = ver->older) {
			exports = ver->exports;
			if (ver->cb->modid == modid && exports->libnid == libnid) {
				info->version = exports->version;
				info->attr = exports->attr;
				info->libnid = libnid;
				info->libname = exports->libname;
				info->nfunc = exports->nfunc;
				info->nvar = exports->nvar;
				info->nidtable = exports->nidtable;
				info->entrytable = exports->entrytable;
				ret = 0;
				goto out;
			}
		}
	}
	ret = SCE_KERNEL_ERROR_LIBRARYDB_NO_LIB;
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return ret;
}

/* name unknown */
LibStubCB *LibraryDBGetLostClientHead(SceUID pid)
{
	LibStubCB *head;
	LibraryDB *db;
	int intr;

	db = LibraryDBLock(pid, &intr);
	if (db == NULL) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 1126);
		return NULL;
	}
	head = db->lostClients;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return head;
}

int LibraryDBGetModuleInfoById(SceUID pid, SceUID modid, SceKernelModuleInfo *info)
{
	SceModuleObject *obj;
	SceModuleCB *cb;
	SceModuleSegment *seg;
	LibraryDB *db;
	int ret, intr, i;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 1240);
	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	obj = ModulemgrReferModule(modid);
	if (obj == NULL) {
		ret = SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD;
		goto unlock;
	}
	cb = &obj->cb;
	if (modid != cb->modid) {
		ret = SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD;
		goto release;
	}
	info->modid = pid == KERNEL_PID ? modid : cb->puid;
	info->modattr = cb->attr;
	info->modver[0] = cb->version[0];
	info->modver[1] = cb->version[1];
	__builtin_memcpy(info->module_name, cb->name, 26);
	switch (cb->status) {
	case MODULE_STATUS_LOADED:
	case MODULE_STATUS_STARTING:
	case MODULE_STATUS_START_SUSPENDED:
		info->state = SCE_KERNEL_MODULE_STATE_LOADED;
		break;
	case MODULE_STATUS_STARTED:
		info->state = SCE_KERNEL_MODULE_STATE_STARTED;
		break;
	default:
		info->state = SCE_KERNEL_MODULE_STATE_STOPPED;
		break;
	}
	info->start_entry = cb->moduleStart;
	info->stop_entry = cb->moduleStop;
	info->exit_entry = cb->moduleExit;
	info->exidx_top = cb->exidxTop;
	info->exidx_btm = cb->exidxBtm;
	info->extab_top = cb->extabStart;
	info->extab_btm = cb->extabEnd;
	info->tlsInit = cb->tlsStart;
	info->tlsInitSize = cb->tlsFilesz;
	info->tlsAreaSize = cb->tlsMemsz;
	strncpy(info->path, cb->path, sizeof(info->path));
	for (i = 0; i < cb->nSegments; i++) {
		seg = &cb->segment[i];
		info->segments[i].size = sizeof(info->segments[i]);
		info->segments[i].perms = seg->perms | seg->extraPages << 20;
		info->segments[i].vaddr = seg->vaddr;
		info->segments[i].memsz = seg->memsz;
		info->segments[i].filesz = seg->filesz;
	}
	for (; i < 4; i++) {
		info->segments[i].perms = 0;
		info->segments[i].vaddr = NULL;
		info->segments[i].memsz = 0;
		info->segments[i].filesz = 0;
	}
	ret = 0;
release:
	ModulemgrReleaseModule(modid);
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return ret;
}

/* name unknown */
int LibraryDBGetModuleCBList(SceUID pid, SceModuleCB **list, int *count)
{
	SceModuleCB *cb;
	LibraryDB *db;
	int n, max, intr;

	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	max = *count;
	cb = db->modules;
	n = 0;
	if (max > 0 && cb != NULL) {
		do {
			list[n++] = cb;
			cb = cb->next;
		} while (n != max && cb != NULL);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	*count = n;
	return 0;
}

int sceKernelGetLostLibraryInfo(SceUID pid, SceUID modid, SceNID libnid,
				SceKernelLostLibraryInfo *info)
{
	SceModuleImports *imports;
	LibStubCB *stub;
	LibraryDB *db;
	SceUID id;
	int ret, intr;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 1763);
	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	for (stub = db->lostClients; stub != NULL; stub = stub->next) {
		id = pid == KERNEL_PID ? stub->cb->modid : stub->cb->puid;
		if (stub->imports->libnid != libnid || id != modid)
			continue;
		info->libname[0xff] = 0;
		info->libnid = libnid;
		info->modid = id;
		info->version = stub->imports->version;
		strncpy(info->libname, stub->imports->libname, 0xff);
		imports = stub->imports;
		info->nfunc = imports->nfunc;
		info->nvar = imports->nvar;
		info->ntls = 0;
		info->funcnid = imports->funcnid;
		info->funcent = imports->funcent;
		info->varnid = imports->varnid;
		info->varent = imports->varent;
		ret = 0;
		goto out;
	}
	ret = SCE_KERNEL_ERROR_LIBRARYDB_NO_LIB;
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return ret;
}

/* name unknown: the module whose segment holds addr */
int LibraryDBFindModuleByAddr(LibraryDB *db, SceUInt32 addr, SceModuleCB **out)
{
	Addr2ModTable *tbl = db->addr2mod;
	Addr2ModExidx *exidx;
	SceModuleCB **ent, *cb;
	SceUInt16 i;

	if (tbl != NULL && (ent = Addr2ModGetEntry(tbl->table, &tbl->exidx, addr, &exidx)) != NULL &&
	    (cb = *ent) != NULL && addr - (SceUInt32)cb->segment[0].vaddr < cb->segment[0].memsz) {
		*out = cb;
		return 0;
	}
	for (cb = db->modules; cb != NULL; cb = cb->next) {
		for (i = 0; i < cb->nSegments; i++) {
			if (addr - (SceUInt32)cb->segment[i].vaddr < cb->segment[i].memsz) {
				*out = cb;
				return 0;
			}
		}
	}
	return SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD;
}

int SceModulemgrForKernel_F1DE6949(SceUID pid, SceUInt32 addr)
{
	SceModuleCB *cb = NULL;
	LibraryDB *db;
	int ret = 0, intr;

	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return 0;
	if (LibraryDBFindModuleByAddr(db, addr, &cb) >= 0)
		ret = (cb->flags & MODULE_FLAG_1000) != 0;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return ret;
}

int SceModulemgrForKernel_1728612F(SceUID pid, SceUInt32 addr, SceModuleCB **out)
{
	LibraryDB *db;
	int ret, intr;

	if (pid == 0)
		pid = sceKernelGetProcessId();
	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	ret = LibraryDBFindModuleByAddr(db, addr, out);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return ret;
}

/* name unknown: the module GUID at addr */
SceUID LibraryDBGetModuleUIDByAddr(SceUID pid, SceUInt32 addr)
{
	SceModuleCB *cb;
	LibraryDB *db;
	int ret, intr;

	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	ret = LibraryDBFindModuleByAddr(db, addr, &cb);
	if (ret == 0)
		ret = cb->modid;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return ret;
}

/* name unknown: the module id at addr, as the process sees it */
SceUID LibraryDBGetModuleIdByAddr(SceUID pid, SceUInt32 addr)
{
	SceModuleObject *obj;
	SceUID modid, puid;

	if (pid == 0)
		pid = sceKernelGetProcessId();
	modid = LibraryDBGetModuleUIDByAddr(pid, addr);
	if (pid != KERNEL_PID && modid > 0) {
		obj = ModulemgrReferModule(modid);
		if (obj == NULL)
			return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
		puid = obj->cb.puid;
		ModulemgrReleaseModule(modid);
		return puid;
	}
	return modid;
}

/* name unknown */
SceUID LibraryDBFindModuleByName(SceUID pid, const char *name)
{
	SceModuleCB *cb;
	LibraryDB *db;
	int ret, intr;

	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	for (cb = db->modules; cb != NULL; cb = cb->next) {
		if (strncmp(cb->name, name, 0x1a) == 0) {
			ret = cb->modid;
			goto out;
		}
	}
	ret = SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD;
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return ret;
}

SceUID sceKernelGetModuleIdByPid(SceUID pid)
{
	SceModuleObject *obj;
	LibraryDB *db;
	int ret, intr;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 2031);
	if (pid == KERNEL_PID || (db = LibraryDBLock(pid, &intr)) == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD;
	obj = ModulemgrReferModule(db->mainModid);
	if (obj == NULL) {
		ret = SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD;
	} else {
		if ((obj->cb.flags & MODULE_FLAG_PROCESS_IMAGE) != 0 && obj->cb.pid == pid)
			ret = db->mainModid;
		else
			ret = SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD;
		ModulemgrReleaseModule(db->mainModid);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return ret;
}

/* the import whose (tool copied) function stubs hold addr */
LibStubCB *LibraryDBGetStubCBByStubCodeAddrForTool(SceModuleCB *cb, SceUInt32 addr)
{
	SceUID pid = cb->pid;
	LibStubCB *stub;
	LibraryDB *db;
	int i, j, intr;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 2067);
	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return NULL;
	stub = cb->libStubCB;
	for (i = 0; i < cb->libStubCount; i++, stub++) {
		SCE_DBG_ASSERT_LEVEL(0, pid == stub->cb->pid, 2077);
		for (j = 0; j < stub->imports->nfunc; j++) {
			if ((SceUInt32)stub->toolCopy[j] == addr)
				goto out;
		}
	}
	stub = NULL;
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return stub;
}

int sceKernelGetLibraryDBFlags(SceUID pid, SceUInt32 *flags)
{
	LibraryDB *db;
	int intr;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 2100);
	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	*flags = db->flags;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return 0;
}

/* name unknown */
int LibraryDBIsAslr(SceUID pid)
{
	return (LibraryDBGetDB(pid)->flags & (LIBDB_FLAG_PCSG00307 | LIBDB_FLAG_ASLR)) ==
	       LIBDB_FLAG_ASLR;
}

/* name unknown */
void LibraryDBSetAslr(SceUID pid)
{
	LibraryDBGetDB(pid)->flags |= LIBDB_FLAG_ASLR;
}

void LibraryDBRenameAddr2ModTbls(SceUID pid, const char *name)
{
	LibraryDB *db;
	int intr;

	db = LibraryDBLock(pid, &intr);
	if (db == NULL) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 2133);
		return;
	}
	SceSysmemForDriver_4CFA4100(db->addr2mod->tableBlock, name);
	SceSysmemForDriver_4CFA4100(db->addr2mod->exidxKernelBlock, name);
	SceSysmemForDriver_4CFA4100(db->addr2mod->exidxBlock, name);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
}

int SceModulemgrForKernel_5564A860(SceUID pid, SceUInt32 addr, SceUInt32 *fingerprint,
				     void **textBase, char *name)
{
	SceModuleCB *cb;
	LibraryDB *db;
	int ret, intr;

	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD;
	ret = LibraryDBFindModuleByAddr(db, addr, &cb);
	if (ret >= 0) {
		if (fingerprint != NULL)
			*fingerprint = cb->fingerprint;
		if (textBase != NULL)
			*textBase = cb->segment[0].vaddr;
		if (name != NULL)
			strncpy(name, cb->name, 0x1b);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return ret;
}

/* name unknown */
int LibraryDBIsShared(SceUID pid)
{
	return LibraryDBGetDB(pid)->flags & LIBDB_FLAG_SHARED;
}

/* name unknown: add a module; the original of a shared module is remembered by path */
void LibraryDBRegisterModule(SceModuleCB *cb)
{
	SharedModuleEntry *ent;
	SceModuleSegment *seg;
	LibraryDB *db;
	SceUID pid;
	void *copy;
	int intr, lock;

	db = LibraryDBLock(cb->pid, &intr);
	if (db == NULL) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 623);
		return;
	}
	cb->next = db->modules;
	db->moduleCount++;
	db->modules = cb;
	if (db->addr2mod != NULL)
		Addr2ModRegister(db->addr2mod, cb);

	if ((cb->flags & MODULE_FLAG_SHARED_ORIGINAL) != 0) {
		pid = cb->pid;
		if (sceKernelGetProcessType(pid) != PROCESS_TYPE_SHARED || LibraryDBIsShared(pid)) {
			cb->flags &= ~MODULE_FLAG_SHARED_ORIGINAL;
		} else {
			ent = SceSysmemForKernel_85571907(sizeof(*ent));
			if (ent == NULL)
				goto unlock;
			ent->cb = cb;
			ent->dataCopy = NULL;
			ent->refCount = 0;
			seg = &cb->segment[cb->nSegments - 1];
			if ((seg->perms & SEGMENT_PERM_WRITE) != 0 && seg->vaddr != NULL &&
			    seg->filesz != 0) {
				copy = SceSysmemForKernel_85571907(seg->filesz);
				if (copy == NULL)
					goto unlock;
				if (sceKernelCopyFromUserProc(pid, copy, seg->vaddr, seg->filesz) < 0) {
					SceSysmemForKernel_4233C16D(copy);
					goto unlock;
				}
				ent->dataCopy = copy;
			}
			lock = SceCpuForDriver_D32ACE9E(&g_sharedLock);
			ent->next = g_sharedModules;
			g_sharedModules = ent;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&g_sharedLock, lock);
		}
	}
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
}

/* name unknown */
void LibraryDBSetShared(SceUID pid)
{
	LibraryDBGetDB(pid)->flags |= LIBDB_FLAG_SHARED;
	if (sceKernelGetProcessType(pid) == PROCESS_TYPE_SHARED)
		g_sharedFlag = 1;
}

int SceModulemgrForKernel_79E761B5(SceUID modid, char *path, SceSize size)
{
	SceModuleObject *obj;

	obj = ModulemgrReferModule(modid);
	if (obj == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD;
	strncpy(path, obj->cb.path, size);
	ModulemgrReleaseModule(modid);
	return 0;
}

int SceModulemgrForKernel_337A3908(SceUID modid, SceUInt32 *fingerprint)
{
	SceModuleObject *obj;

	obj = ModulemgrReferModule(modid);
	if (obj == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD;
	*fingerprint = obj->cb.fingerprint;
	ModulemgrReleaseModule(modid);
	return 0;
}

int sceKernelGetLibEntCBListForSyslibtrace(LibEntCB **list, SceSize *num)
{
	LibEntCB *lib, *ver;
	LibraryDB *db;
	SceSize n, max;
	int intr;

	SCE_DBG_ASSERT(list != NULL, 2233);
	db = LibraryDBLock(KERNEL_PID, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	n = 0;
	max = *num;
	for (lib = db->libList; n < max && lib != NULL; lib = lib->next) {
		for (ver = lib; n != max && ver != NULL; ver = ver->older)
			list[n++] = ver;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	*num = n;
	return 0;
}

static inline int getNumOfLostLibrariesInModule(SceUID pid, SceUID modid)
{
	LibStubCB *stub;
	LibraryDB *db;
	SceUID id;
	int n, intr;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 1682);
	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	n = 0;
	for (stub = db->lostClients; stub != NULL; stub = stub->next) {
		id = pid == KERNEL_PID ? stub->cb->modid : stub->cb->puid;
		if (id == modid)
			n++;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return n;
}

static inline int getLostLibrariesInModule(SceUID pid, SceUID modid,
					   SceKernelLostLibraryEntry *list, SceSize *num)
{
	LibStubCB *stub;
	LibraryDB *db;
	SceSize n;
	SceUID id;
	int intr;

	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	n = 0;
	for (stub = db->lostClients; n < *num && stub != NULL; stub = stub->next) {
		id = pid == KERNEL_PID ? stub->cb->modid : stub->cb->puid;
		if (id == modid) {
			list[n].libnid = stub->imports->libnid;
			list[n].modid = modid;
			n++;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	*num = n;
	return 0;
}

int sceKernelGetLostLibraryListInModule(SceUID pid, SceUID modid, SceKernelLostLibraryEntry *list,
					SceSize *num)
{
	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 2258);
	if (list == NULL)
		return getNumOfLostLibrariesInModule(pid, modid);
	return getLostLibrariesInModule(pid, modid, list, num);
}

int sceKernelGetLostLibraryList(SceUID pid, SceKernelLostLibraryEntry *list, SceSize *num)
{
	LibStubCB *stub;
	LibraryDB *db;
	SceSize n;
	int intr;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 2269);
	if (list == NULL) {
		db = LibraryDBLock(pid, &intr);
		n = db->lostCount;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
		return n;
	}
	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	n = 0;
	for (stub = db->lostClients; n < *num && stub != NULL; stub = stub->next) {
		list[n].libnid = stub->imports->libnid;
		list[n].modid = pid == KERNEL_PID ? stub->cb->modid : stub->cb->puid;
		n++;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	*num = n;
	return 0;
}

int sceKernelGetLibraryList(SceUID pid, SceUID *list, SceSize *num)
{
	LibEntCB *lib, *ver;
	LibraryDB *db;
	SceSize n;
	int intr;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 2280);
	if (list == NULL) {
		db = LibraryDBLock(pid, &intr);
		if (db == NULL)
			return 0;
		n = db->libCount;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
		return n;
	}
	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	n = 0;
	for (lib = db->libList; n < *num && lib != NULL; lib = lib->next) {
		for (ver = lib; n < *num && ver != NULL; ver = ver->older)
			list[n++] = GetLibUIDByLibEntCB(pid, &ver->guid, &ver->puid);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	*num = n;
	return 0;
}

static inline int getNumOfModulesWithType(SceUID pid, SceUInt32 flags)
{
	SceModuleCB *cb;
	LibraryDB *db;
	int n, intr;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 1156);
	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return 0;
	n = 0;
	for (cb = db->modules; cb != NULL; cb = cb->next) {
		if (pid != KERNEL_PID) {
			if ((flags & 1) != 0 && (cb->flags & MODULE_FLAG_1000) == 0)
				n++;
			if ((flags & 0x80) == 0 || (cb->flags & MODULE_FLAG_1000) == 0)
				continue;
		}
		n++;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	return n;
}

static inline SceSize putModuleId(SceUID *list, SceSize n, SceModuleCB *cb, SceUInt32 type)
{
	if ((type & 1) != 0)
		list[n++] = cb->modid;
	else if ((type & 2) != 0)
		list[n++] = cb->puid;
	return n;
}

static inline int getModuleList(SceUID pid, SceUInt32 flags, SceUInt32 type, SceUID *list,
				SceSize *num)
{
	SceModuleCB *cb;
	LibraryDB *db;
	SceSize n;
	int intr;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 969);
	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	n = 0;
	for (cb = db->modules; n < *num && cb != NULL; cb = cb->next) {
		if (pid != KERNEL_PID) {
			if ((flags & 1) != 0 && (cb->flags & MODULE_FLAG_1000) == 0)
				n = putModuleId(list, n, cb, type);
			if ((flags & 0x80) == 0 || (cb->flags & MODULE_FLAG_1000) == 0)
				continue;
		}
		n = putModuleId(list, n, cb, type);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	*num = n;
	return 0;
}

int sceKernelGetModuleList(SceUID pid, SceUInt32 flags, SceUInt32 type, SceUID *list,
			   SceSize *num)
{
	if (pid == 0)
		pid = sceKernelGetProcessId();
	if (list == NULL)
		return getNumOfModulesWithType(pid, flags);
	return getModuleList(pid, flags, type, list, num);
}

int SceModulemgrForDriver_1D9E0F7E(SceUInt32 addr, SceKernelModuleInfo *info)
{
	SceUID modid;

	modid = LibraryDBGetModuleIdByAddr(KERNEL_PID, addr);
	if (modid < 0)
		return modid;
	return LibraryDBGetModuleInfoById(KERNEL_PID, modid, info);
}

int SceModulemgrForKernel_DAA90093(SceUID pid, SceUID modid, SceKernelModuleInfo *info)
{
	if (pid == 0)
		pid = sceKernelGetProcessId();
	return LibraryDBGetModuleInfoById(pid, modid, info);
}

SceUID sceKernelGetModuleIdByAddrForDebugger(SceUID pid, SceUInt32 addr)
{
	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 2324);
	return LibraryDBGetModuleIdByAddr(pid, addr);
}

int sceKernelGetModuleInfoForDebugger(SceUID pid, SceKernelModuleInfoForDebugger *info, int *num)
{
	SceKernelSegmentInfoForDebugger *dst;
	SceKernelModuleExidxInfo *exidx;
	SceModuleSegment *seg;
	SceModuleCB *cb;
	LibraryDB *db;
	int n, i, intr;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 2333);
	if (info == NULL) {
		db = LibraryDBLock(pid, &intr);
		if (db == NULL)
			return 0;
		n = db->moduleCount;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
		return n;
	}
	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	cb = db->modules;
	for (n = 0; n < *num && cb != NULL; n++, cb = cb->next) {
		info->modid = pid == KERNEL_PID ? cb->modid : cb->puid;
		info->sdkVersion = cb->sdkVersion;
		info->version = cb->version[0] | cb->version[1] << 8;
		info->reserved0 = 0;
		info->type = (cb->flags & MODULE_FLAG_SELF_FIXED) != 0 ? 0x20 : 0x10;
		info->moduleStart = cb->moduleStart;
		info->moduleStop = cb->moduleStop;
		info->moduleExit = cb->moduleExit;
		info->flags = cb->flags;
		__builtin_memcpy(info->name, cb->name, 26);
		switch (cb->status) {
		case MODULE_STATUS_LOADED:
			info->status = 0;
			break;
		case MODULE_STATUS_STARTING:
		case MODULE_STATUS_START_SUSPENDED:
			info->status = 1;
			break;
		case MODULE_STATUS_STARTED:
			info->status = 2;
			break;
		case MODULE_STATUS_STOPPING:
			info->status = 3;
			break;
		default:
			info->status = 4;
			break;
		}
		info->nSegments = cb->nSegments;
		info->fingerprint = cb->fingerprint;
		info->size = sizeof(*info) + cb->nSegments * sizeof(info->segment[0]) +
			     sizeof(*exidx);
		memset(info->segment, 0, cb->nSegments * sizeof(info->segment[0]));
		for (i = 0; i < cb->nSegments; i++) {
			seg = &cb->segment[i];
			dst = &info->segment[i];
			dst->size = sizeof(*dst);
			dst->perms = seg->perms | seg->extraPages << 20;
			dst->vaddr = seg->vaddr;
			dst->memsz = seg->memsz;
			dst->align = 1 << (seg->alignLog2 & 0xff);
		}
		exidx = (SceKernelModuleExidxInfo *)&info->segment[i];
		exidx->exidxTop = cb->exidxTop;
		exidx->exidxBtm = cb->exidxBtm;
		exidx->extabStart = cb->extabStart;
		exidx->extabEnd = cb->extabEnd;
		info = (SceKernelModuleInfoForDebugger *)(exidx + 1);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	*num = n;
	return 0;
}

int sceKernelGetExportedLibraryListInModule(SceUID pid, SceUID modid, SceUID *list, SceSize *num)
{
	LibEntCB *lib, *ver;
	LibraryDB *db;
	SceSize n;
	SceUID id;
	int intr;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 2344);
	if (list == NULL) {
		db = LibraryDBLock(pid, &intr);
		if (db == NULL)
			return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
		n = 0;
		for (lib = db->libList; lib != NULL; lib = lib->next) {
			for (ver = lib; ver != NULL; ver = ver->older) {
				id = pid == KERNEL_PID ? ver->cb->modid : ver->cb->puid;
				if (id == modid)
					n++;
			}
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
		return n;
	}
	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	n = 0;
	for (lib = db->libList; n < *num && lib != NULL; lib = lib->next) {
		for (ver = lib; n < *num && ver != NULL; ver = ver->older) {
			id = pid == KERNEL_PID ? ver->cb->modid : ver->cb->puid;
			if (id == modid)
				list[n++] = GetLibUIDByLibEntCB(pid, &ver->guid, &ver->puid);
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	*num = n;
	return 0;
}

static inline int getImportedLibrariesInModule(SceUID pid, SceUID modid, SceUID *list,
					       SceSize *num)
{
	SceModuleObject *obj;
	LibStubCB *stub;
	LibraryDB *db;
	int n, i, intr;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 1549);
	if (list == NULL) {
		db = LibraryDBLock(pid, &intr);
		if (db == NULL)
			return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
		obj = ModulemgrReferModule(modid);
		if (obj == NULL) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
			return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
		}
		stub = obj->cb.libStubCB;
		n = 0;
		for (i = 0; i < obj->cb.libStubCount; i++, stub++) {
			if (stub->guid != 0 && stub->linked != NULL)
				n++;
		}
		ModulemgrReleaseModule(modid);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
		return n;
	}
	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	obj = ModulemgrReferModule(modid);
	if (obj == NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
		return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
	}
	stub = obj->cb.libStubCB;
	n = 0;
	for (i = 0; i < obj->cb.libStubCount; i++, stub++) {
		if (stub->guid != 0 && stub->linked != NULL)
			list[n++] = GetLibUIDByLibEntCB(pid, &stub->linked->guid,
							&stub->linked->puid);
	}
	ModulemgrReleaseModule(modid);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
	*num = n;
	return 0;
}

int sceKernelGetImportedLibraryListInModule(SceUID pid, SceUID modid, SceUID *list, SceSize *num)
{
	int ret;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 2356);
	ret = ModulemgrReferProcess(pid);
	if (ret < 0)
		return ret;
	ret = getImportedLibrariesInModule(pid, modid, list, num);
	ModulemgrReleaseProcess(pid);
	return ret;
}

int sceKernelGetLibraryInfoForDebugger(SceUID pid, SceUID libid,
				       SceKernelLibraryInfoForDebugger *info)
{
	SceLibraryObject *obj;
	SceModuleExports *exports;
	LibEntCB *ent;
	int ret;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 2373);
	ret = ModulemgrReferProcess(pid);
	if (ret < 0)
		return ret;
	obj = LibraryDBReferLibrary(pid, libid);
	if (obj == NULL) {
		ret = SCE_KERNEL_ERROR_MODULEMGR_NOENT;
	} else {
		ent = obj->libEntCB;
		exports = ent->exports;
		info->libid = ent->guid == libid ? ent->guid : ent->puid;
		info->libnid = exports->libnid;
		info->version = exports->version;
		info->attr = exports->attr;
		info->nfunc = exports->nfunc;
		info->nvar = exports->nvar;
		info->ntls = 0;
		info->clientCount = ent->clientCount;
		info->modid = pid == KERNEL_PID ? ent->cb->modid : ent->cb->puid;
		info->libname[0xff] = 0;
		strncpy(info->libname, ent->exports->libname, 0xff);
		ModulemgrReleaseModule(obj->modid);
		sceGUIDReleaseObject(obj->libEntCB->guid);
		ret = 0;
	}
	ModulemgrReleaseProcess(pid);
	return ret;
}

int sceKernelGetLibraryExportInfoForDebugger(SceUID pid, SceUID libid,
					     SceKernelLibraryExportEntry *list, SceSize *num,
					     SceUInt32 start)
{
	SceLibraryObject *obj;
	SceModuleExports *exports;
	LibEntCB *ent;
	SceUInt32 i;
	SceSize n;
	int ret;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 2390);
	ret = ModulemgrReferProcess(pid);
	if (ret < 0)
		return ret;
	obj = LibraryDBReferLibrary(pid, libid);
	if (obj == NULL) {
		ret = SCE_KERNEL_ERROR_MODULEMGR_NOENT;
		goto out;
	}
	ent = obj->libEntCB;
	n = 0;
	ret = 0;
	for (i = 0; n < *num && (exports = ent->exports, i < exports->nfunc + exports->nvar); i++) {
		if (i + 1 < start)
			continue;
		ret = ModulemgrCopyIn(pid, &list[n].nid, &exports->nidtable[i], sizeof(SceNID));
		n++;
		if (ret < 0)
			goto release;
		ret = ModulemgrCopyIn(pid, &list[n - 1].entry, &ent->exports->entrytable[i],
				      sizeof(void *));
		if (ret < 0)
			goto release;
	}
	*num = n;
release:
	ModulemgrReleaseModule(obj->modid);
	sceGUIDReleaseObject(obj->libEntCB->guid);
out:
	ModulemgrReleaseProcess(pid);
	return ret;
}

int sceKernelGetLibraryClientList(SceUID pid, SceUID libid, SceUID *list, SceSize *num,
				  SceUInt32 start)
{
	SceLibraryObject *obj;
	LibStubCB *stub;
	LibraryDB *db;
	SceUInt32 i;
	SceSize n;
	SceUID guid;
	int ret, intr;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 2407);
	ret = ModulemgrReferProcess(pid);
	if (ret < 0)
		return ret;
	if (list == NULL) {
		obj = LibraryDBReferLibrary(pid, libid);
		if (obj == NULL) {
			ret = SCE_KERNEL_ERROR_MODULEMGR_NOENT;
		} else {
			ret = obj->libEntCB->clientCount;
			ModulemgrReleaseModule(obj->modid);
			sceGUIDReleaseObject(obj->libEntCB->guid);
		}
	} else {
		guid = GetLibGUIDByLibUID(pid, libid);
		ret = guid;
		if (guid >= 0) {
			db = LibraryDBLock(pid, &intr);
			if (db == NULL) {
				ret = SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
			} else {
				ret = SceSysmemForDriver_00ED6C14(guid, g_libraryClass, (SceObjectBase **)&obj);
				if (ret >= 0) {
					n = 0;
					if (obj->libEntCB->clientCount > 0) {
						i = 0;
						for (stub = obj->libEntCB->clients; stub != NULL;
						     stub = stub->next) {
							if (++i < start)
								continue;
							list[n++] = libid == guid ? stub->cb->modid :
										    stub->cb->puid;
							if (n >= *num)
								break;
						}
					}
					sceGUIDReleaseObject(guid);
					*num = n;
				}
				sceKernelSpinlockLowUnlockCpuResumeIntr(&db->lock, intr);
			}
		}
	}
	ModulemgrReleaseProcess(pid);
	return ret;
}
