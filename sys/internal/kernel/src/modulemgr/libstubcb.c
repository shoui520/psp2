/*
 * libstubcb.c - the imported libraries of a module (LibStubCB)
 */

#include "modulemgr_internal.h"

/* what sceKernelGetStubInfoForDebugger reports */
typedef struct SceKernelStubInfoForDebugger {
	SceSize size;
	SceUID stubid;
	SceUID modid;
	SceNID libnid;
	SceUInt16 version;
	SceUInt16 attr;
	SceUInt16 nfunc;
	SceUInt16 nvar;
	SceUInt16 ntls;
	SceUInt16 reserved;
	char libname[0x100];
	SceUID libPid;			/* the library the stub is linked to */
	SceUID libModid;
	SceUID libid;
} SceKernelStubInfoForDebugger;

typedef struct SceKernelStubNidEntry {
	SceNID nid;
	void *entry;
} SceKernelStubNidEntry;

/* name unknown: bring a 0x24- or 0x34-byte import entry into the 0x24-byte form */
SceModuleImports *LibStubNormalize(SceModuleImports *dst, const void *src)
{
	const SceModuleImports34 *imp = src;

	if (imp->size == sizeof(SceModuleImports)) {
		*dst = *(const SceModuleImports *)src;
		return dst;
	}
	dst->size = sizeof(SceModuleImports);
	dst->version = imp->version;
	dst->attr = imp->attr;
	dst->nfunc = imp->nfunc;
	dst->nvar = imp->nvar;
	dst->libnid = imp->libnid;
	dst->libname = imp->libname;
	dst->funcnid = imp->funcnid;
	dst->funcent = imp->funcent;
	dst->varnid = imp->varnid;
	dst->varent = imp->varent;
	return dst;
}

int LibStub_checkStubTable(LoadelfContext *ctx)
{
	SceModuleCB *cb = ctx->cb;
	SceModuleImports tmp, *imp;
	SceUInt8 *p, *end;
	SceUInt32 bias, count;
	SceUInt8 esize;
	SceSize n;

	if ((cb->flags & MODULE_FLAG_SHARED_MASK) != 0) {
		SCE_DBG_ASSERT_LEVEL(0, cb->shared != NULL, 73);
		cb->libStubCount = cb->shared->cb->libStubCount;
		ctx->libStubCBSize = cb->libStubCount * sizeof(LibStubCB);
		return 0;
	}

	bias = Loadelf_GetAddrBias(ctx, cb->libstubTop);
	end = (SceUInt8 *)cb->libstubBtm + bias;
	p = (SceUInt8 *)cb->libstubTop + bias;
	if (end <= p) {
		cb->libStubCount = 0;
		ctx->libStubCBSize = 0;
		return 0;
	}
	esize = *p;
	if (esize == sizeof(SceModuleImports))
		count = (SceUInt32)(end - p) / sizeof(SceModuleImports);
	else if (esize == sizeof(SceModuleImports34))
		count = (SceUInt32)(end - p) / sizeof(SceModuleImports34);
	else
		return SCE_KERNEL_ERROR_MODULEMGR_INVALID_STUB;
	if (count > 0xffff)
		return SCE_KERNEL_ERROR_MODULEMGR_INVALID_STUB;
	cb->libStubCount = count;

	while (esize == *p) {
		imp = LibStubNormalize(&tmp, p);
		if (!ModuleCBIsInReadOnly(cb, (SceUInt32)imp->libname, 1) ||
		    !ModuleCBIsInReadOnly(cb, (SceUInt32)imp->libname,
					  strnlen((const char *)imp->libname + bias, 0x100)))
			return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
		if (imp->nfunc != 0) {
			n = imp->nfunc * 4;
			if (!ModuleCBIsInReadOnly(cb, (SceUInt32)imp->funcnid, n) ||
			    !ModuleCBIsInReadOnly(cb, (SceUInt32)imp->funcent, n))
				return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
		}
		if (imp->nvar != 0) {
			n = imp->nvar * 4;
			if (!ModuleCBIsInReadOnly(cb, (SceUInt32)imp->varnid, n) ||
			    !ModuleCBIsInReadOnly(cb, (SceUInt32)imp->varent, n))
				return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
		}
		if (cb->pid != KERNEL_PID || esize != sizeof(SceModuleImports))
			ctx->libstubSize = count * sizeof(SceModuleImports);
		p += *p;
		if (end <= p) {
			ctx->libStubCBSize = count * sizeof(LibStubCB);
			return 0;
		}
	}
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 115, "illegal stubtable element size %d byte\n", *p);
	return SCE_KERNEL_ERROR_MODULEMGR_INVALID_STUB;
}

int LibStubCBInit(LoadelfContext *ctx)
{
	SceModuleCB *cb = ctx->cb;
	SceUInt32 offset = ctx->libstubOffset;
	SceUInt16 count = cb->libStubCount;
	SceModuleImports *dst, *imp;
	SceModuleCB *c;
	LibStubCB *stub;
	SceUInt8 *p, *end;
	SceUInt32 bias, i, n;
	SceUInt16 nfunc, nvar;
	const char *name;
	char *interned;
	void **copy;
	SceUID pid;

	if (cb->pid == KERNEL_PID) {
		p = cb->libstubTop;
		end = cb->libstubBtm;
		if (p == end || *p == sizeof(SceModuleImports)) {
			cb->libstub = NULL;
		} else {
			dst = (SceModuleImports *)((SceUInt8 *)cb->linkInfo + offset);
			offset += ctx->libstubSize;
			cb->libstub = dst;
			for (; p < end; p += n, dst++) {
				n = *p;
				LibStubNormalize(dst, p);
			}
		}
		bias = 0;
	} else if ((cb->flags & MODULE_FLAG_SHARED_MASK) == 0) {
		bias = Loadelf_GetAddrBias(ctx, cb->libstubTop);
		end = (SceUInt8 *)cb->libstubBtm + bias;
		dst = (SceModuleImports *)((SceUInt8 *)cb->linkInfo + offset);
		offset += ctx->libstubSize;
		cb->libstub = dst;
		for (p = (SceUInt8 *)cb->libstubTop + bias; p < end; p += n, dst++) {
			n = *p;
			LibStubNormalize(dst, p);
			name = dst->libname + bias;
			if (SceSysmemForKernel_B628B46B(name, memchr(name, 0, 0x1f) == NULL ? 0x10000 : 0,
						       &interned) < 0) {
				dst->libname = NULL;
				return SCE_KERNEL_ERROR_MODULEMGR_NOMEM;
			}
			dst->libname = interned;
			if (interned == NULL)
				return SCE_KERNEL_ERROR_MODULEMGR_NOMEM;
		}
	} else {
		bias = 0;
		cb->libstub = cb->shared->cb->libstub;
	}

	if (ctx->libStubCBSize == 0)
		return 0;
	stub = (LibStubCB *)((SceUInt8 *)cb->linkInfo + offset);
	cb->libStubCB = stub;
	memset(stub, 0, ctx->libStubCBSize);
	imp = cb->libstub;
	if (imp == NULL)
		imp = cb->libstubTop;
	stub = cb->libStubCB;
	for (i = 0; i < count; i++, stub++, imp++) {
		stub->next = NULL;
		stub->imports = imp;
		stub->linked = NULL;
		stub->cb = cb;
		stub->guid = 0;
		stub->toolCopy = NULL;
		if (cb->pid == KERNEL_PID)
			continue;
		c = ctx->cb;
		pid = c->pid;
		if ((c->flags & MODULE_FLAG_SHARED_MASK) != 0) {
			/* the same import of the original module */
			stub->toolCopy = c->shared->cb->libStubCB[stub - cb->libStubCB].toolCopy;
			continue;
		}
		nfunc = imp->nfunc;
		nvar = imp->nvar;
		if (!sceKernelCheckDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART) || nfunc + nvar == 0)
			continue;
		copy = sceKernelAllocRemoteToolHeap(pid, (nfunc + nvar) * 8);
		stub->toolCopy = copy;
		if (copy == NULL)
			return SCE_KERNEL_ERROR_MODULEMGR_NOMEM_STUB;
		if (stub->imports->nfunc != 0) {
			memcpy(copy + stub->imports->nfunc, (SceUInt8 *)stub->imports->funcnid + bias,
			       stub->imports->nfunc * 4);
			memcpy(stub->toolCopy, (SceUInt8 *)stub->imports->funcent + bias,
			       stub->imports->nfunc * 4);
		}
		if (stub->imports->nvar != 0) {
			memcpy(stub->toolCopy + stub->imports->nvar + stub->imports->nfunc * 2,
			       (SceUInt8 *)stub->imports->varnid + bias, stub->imports->nvar * 4);
			memcpy(stub->toolCopy + stub->imports->nfunc * 2,
			       (SceUInt8 *)stub->imports->varent + bias, stub->imports->nvar * 4);
		}
	}
	return 0;
}

/* name unknown: release the interned names and the tool heap copies */
void LibStubFree(SceModuleCB *cb)
{
	SceUID pid = cb->pid;
	SceModuleImports *imp;
	LibStubCB *stub;
	int i, count;

	if ((cb->flags & MODULE_FLAG_SHARED_MASK) != 0)
		return;
	imp = cb->libstub;
	count = cb->libStubCount;
	if (imp != NULL && pid != KERNEL_PID) {
		for (i = 0; i < count; i++, imp++) {
			if (imp->libname != NULL)
				SceSysmemForKernel_634D0D0C(imp->libname, 0x1000000);
		}
	}
	stub = cb->libStubCB;
	if (stub == NULL)
		return;
	for (i = 0; i < count; i++, stub++) {
		if (stub->toolCopy != NULL) {
			sceKernelFreeRemoteToolHeap(pid, stub->toolCopy);
			stub->toolCopy = NULL;
		}
	}
}

/* name unknown */
int LibStubClassCreate(void *obj)
{
	return 0;
}

/* name unknown */
int LibStubClassDestroy(void *obj)
{
	((SceLibStubObject *)obj)->modid = 0;
	return 0;
}

/* name unknown */
void LibStubCBClear(LibStubCB *stub)
{
	stub->next = NULL;
	stub->linked = NULL;
	if (stub->guid > 0) {
		sceGUIDClose(stub->guid);
		stub->guid = 0;
	}
}

/* name unknown */
int LibStubCBCreateUID(LibStubCB *stub, SceUInt16 index)
{
	SceLibStubObject *obj;
	SceUID uid;

	uid = sceGUIDKernelCreateWithOpt(g_stubClass, stub->imports->libname, NULL, (SceObjectBase **)&obj);
	if (uid < 0)
		return uid;
	stub->guid = uid;
	obj->modid = stub->cb->modid;
	obj->index = index;
	obj->reserved = 0;
	return 0;
}

/* name unknown: the NID and entry tables of an import, copied in for users */
int LibStubGetNidTable(LibStubCB *stub, LibStubNidTable *info)
{
	SceModuleImports *imp = stub->imports;
	SceModuleCB *cb = stub->cb;
	SceUInt32 nfunc = imp->nfunc, nvar = imp->nvar, i;
	SceUInt8 *copy;
	int local, ret;

	info->funcent = imp->funcent;
	info->varnid = imp->varnid;
	info->varent = imp->varent;
	info->libStubCB = stub;
	info->funcnid = imp->funcnid;
	info->nfunc = nfunc;
	info->nvar = nvar;
	info->copy = NULL;
	if (cb->pid == KERNEL_PID || nfunc + nvar == 0)
		return 0;

	copy = ModulemgrAlloc(cb->pid, (nfunc + nvar) * 8);
	if (copy == NULL)
		return SCE_KERNEL_ERROR_MODULEMGR_NOMEM_STUB;
	info->copy = copy;
	info->funcnid = (SceNID *)copy;
	info->funcent = (void **)(copy + nfunc * 4);
	info->varnid = (SceNID *)(copy + nfunc * 8);
	info->varent = (void **)(copy + nfunc * 8 + nvar * 4);
	local = (stub->cb->flags & MODULE_FLAG_SHARED_MASK) == 0;
	if (nfunc != 0) {
		ret = sceKernelCopyFromUser(copy, stub->imports->funcnid, nfunc * 4);
		if (ret < 0)
			return ret;
		ret = sceKernelCopyFromUser(info->funcent, stub->imports->funcent, nfunc * 4);
		if (ret < 0)
			return ret;
		if (local) {
			for (i = 0; i < nfunc; i++) {
				if (!ModuleCBIsInText(stub->cb, (SceUInt32)info->funcent[i], 0x10))
					return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
			}
		}
	}
	if (nvar != 0) {
		ret = sceKernelCopyFromUser(info->varnid, stub->imports->varnid, nvar * 4);
		if (ret < 0)
			return ret;
		ret = sceKernelCopyFromUser(info->varent, stub->imports->varent, nvar * 4);
		if (ret < 0)
			return ret;
		if (local) {
			for (i = 0; i < nvar; i++) {
				if (info->varent[i] != NULL &&
				    !ModuleCBIsInReadOnly(stub->cb, (SceUInt32)info->varent[i], 4))
					return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
			}
		}
	}
	return 0;
}

/* name unknown: the stub tables were written; drop the cached lines */
void LibStubReleaseNidTable(LibStubNidTable *info)
{
	LibStubCB *stub;
	SceModuleImports *imp;
	SceUInt32 t;

	if (info->copy == NULL)
		return;
	stub = info->libStubCB;
	if ((stub->linked == NULL || (stub->linked->exports->attr & LIBRARY_ATTR_SYSCALL) == 0) &&
	    (stub->imports->attr & 8) != 0)
		goto out;
	t = SceThreadmgrForDriver_47F6DE49();
	imp = stub->imports;
	if (imp->nvar != 0) {
		SceSysmemForKernel_FCB5745A((SceUInt32 *)imp->varent, t, imp->nvar);
		imp = stub->imports;
	}
	if (imp->nfunc != 0)
		SceSysmemForKernel_FCB5745A((SceUInt32 *)imp->funcent, t, imp->nfunc);
out:
	ModulemgrFree(info->libStubCB->cb->pid, info->copy);
}

/* the debugger's id of an import */
static inline SceUID getStubUIDByLibStubCB(SceUID pid, LibStubCB *stub)
{
	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 610);
	return stub->guid;
}

static inline SceUID getStubGUIDByStubUID(SceUID pid, SceUID stubid)
{
	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 596);
	return stubid;
}

int sceKernelGetStubList(SceUID pid, SceUID *list, SceUInt32 *count)
{
	SceModuleCB *cb;
	LibraryDB *db;
	SceUInt32 i, n;
	int intr;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 832);
	if (list == NULL) {
		db = LibraryDBLock(pid, &intr);
		if (db == NULL)
			return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
		n = 0;
		for (cb = db->modules; cb != NULL; cb = cb->next) {
			if (cb->libStubCB == NULL)
				continue;
			for (i = 0; i < cb->libStubCount; i++) {
				if (cb->libStubCB[i].guid != 0)
					n++;
			}
		}
		LibraryDBUnlock(db, intr);
		return n;
	}

	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	n = 0;
	for (cb = db->modules; cb != NULL; cb = cb->next) {
		for (i = 0; n < *count && i < cb->libStubCount; i++) {
			if (getStubUIDByLibStubCB(pid, &cb->libStubCB[i]) != 0)
				list[n++] = getStubUIDByLibStubCB(pid, &cb->libStubCB[i]);
		}
	}
	LibraryDBUnlock(db, intr);
	*count = n;
	return 0;
}

int sceKernelGetStubInfoForDebugger(SceUID pid, SceUID stubid,
				    SceKernelStubInfoForDebugger *info)
{
	SceLibStubObject *sobj;
	SceModuleObject *mobj;
	SceModuleImports *imp;
	SceModuleCB *lcb;
	LibStubCB *stub;
	LibEntCB *ent;
	SceUID guid;
	int ret;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 846);
	ret = ModulemgrReferProcess(pid);
	if (ret < 0)
		return ret;
	guid = getStubGUIDByStubUID(pid, stubid);
	ret = guid;
	if (guid < 0 || (ret = sceGUIDReferObjectWithClass(guid, g_stubClass, (SceObjectBase **)&sobj)) < 0)
		goto out;
	ret = sceGUIDReferObjectWithClass(sobj->modid, g_moduleClass, (SceObjectBase **)&mobj);
	if (ret >= 0) {
		if (sobj->index < mobj->cb.libStubCount) {
			stub = &mobj->cb.libStubCB[sobj->index];
			imp = stub->imports;
			info->stubid = stub->guid;
			info->version = imp->version;
			info->libnid = imp->libnid;
			info->attr = imp->attr;
			info->nfunc = imp->nfunc;
			info->nvar = imp->nvar;
			info->ntls = 0;
			info->modid = pid == KERNEL_PID ? stub->cb->modid : stub->cb->puid;
			ent = stub->linked;
			if (ent == NULL) {
				info->libPid = 0;
				info->libModid = 0;
				info->libid = 0;
			} else {
				lcb = ent->cb;
				info->libPid = lcb->pid;
				info->libModid = lcb->pid == KERNEL_PID ? lcb->modid : lcb->puid;
				info->libid = ent->puid != 0 ? ent->puid : ent->guid;
			}
			info->libname[0xff] = '\0';
			strncpy(info->libname, stub->imports->libname, 0xff);
		} else {
			ret = SCE_KERNEL_ERROR_MODULEMGR_INVALID_STUB;
		}
		sceGUIDReleaseObject(sobj->modid);
	}
	sceGUIDReleaseObject(guid);
out:
	ModulemgrReleaseProcess(pid);
	return ret;
}

static inline int getStubNidTableInfoForDebugger(SceUID pid, SceUID stubid,
						 SceKernelStubNidEntry *table, SceUInt32 *count,
						 SceUInt32 start)
{
	SceLibStubObject *sobj;
	SceModuleObject *mobj;
	SceModuleImports *imp;
	LibStubCB *stub;
	SceUInt32 i, j, n, nfunc;
	void **var;
	SceUID guid;
	int ret;

	guid = getStubGUIDByStubUID(pid, stubid);
	ret = guid;
	if (guid < 0 || (ret = sceGUIDReferObjectWithClass(guid, g_stubClass, (SceObjectBase **)&sobj)) < 0)
		return ret;
	ret = sceGUIDReferObjectWithClass(sobj->modid, g_moduleClass, (SceObjectBase **)&mobj);
	if (ret < 0)
		goto release;
	if (sobj->index >= mobj->cb.libStubCount) {
		ret = SCE_KERNEL_ERROR_MODULEMGR_INVALID_STUB;
		goto done;
	}
	stub = &mobj->cb.libStubCB[sobj->index];
	n = 0;
	for (i = 0; n < *count; i++) {
		imp = stub->imports;
		nfunc = imp->nfunc;
		if (i >= nfunc + imp->nvar)
			break;
		if (i + 1 < start)
			continue;
		if (i < nfunc) {
			if (pid == KERNEL_PID) {
				table[n].nid = imp->funcnid[i];
				table[n].entry = imp->funcent[i];
			} else {
				if (stub->toolCopy == NULL) {
					SCE_DBG_ASSERT_LEVEL(0, 0, 793);
					goto done;
				}
				table[n].nid = (SceNID)stub->toolCopy[i + nfunc];
				table[n].entry = stub->toolCopy[i];
			}
		} else {
			j = i - nfunc;
			if (pid == KERNEL_PID) {
				table[n].nid = imp->varnid[j];
				table[n].entry = imp->varent[j];
			} else {
				if (stub->toolCopy == NULL) {
					SCE_DBG_ASSERT_LEVEL(0, 0, 808);
					goto done;
				}
				var = stub->toolCopy + nfunc * 2;
				table[n].nid = (SceNID)var[j + imp->nvar];
				table[n].entry = var[j];
			}
		}
		n++;
	}
	*count = n;
done:
	sceGUIDReleaseObject(sobj->modid);
release:
	sceGUIDReleaseObject(guid);
	return ret;
}

int sceKernelGetStubNidTableForDebugger(SceUID pid, SceUID stubid, SceKernelStubNidEntry *table,
					SceUInt32 *count, SceUInt32 start)
{
	int ret;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 864);
	ret = ModulemgrReferProcess(pid);
	if (ret < 0)
		return ret;
	ret = getStubNidTableInfoForDebugger(pid, stubid, table, count, start);
	ModulemgrReleaseProcess(pid);
	return ret;
}
