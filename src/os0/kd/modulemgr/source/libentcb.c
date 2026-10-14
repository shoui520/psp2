/*
 * libentcb.c - the exported libraries of a module (LibEntCB)
 */

#include "modulemgr_internal.h"

/* hash table size (log2, from the top bit) per export hashinfo nibble */
static const SceUInt8 g_hashShift[8] = { 0, 30, 28, 27, 26, 25, 24, 23 };

int LibEnt_checkEntTable(LoadelfContext *ctx)
{
	SceModuleCB *cb = ctx->cb;
	SceModuleExports *ent, *end;
	SceUInt32 bias, count;
	SceSize libentSize;
	const char *name;

	if ((cb->flags & MODULE_FLAG_SHARED_MASK) != 0) {
		SCE_DBG_ASSERT_LEVEL(0, cb->shared != NULL, 53);
		libentSize = ctx->libentSize;
		count = cb->shared->cb->libEntCount;
		cb->libEntCount = count;
		goto out;
	}

	bias = Loadelf_GetAddrBias(ctx, cb->libentTop);
	ent = (SceModuleExports *)((SceUInt32)cb->libentTop + bias);
	end = (SceModuleExports *)((SceUInt32)cb->libentBtm + bias);
	if (ent->size != sizeof(SceModuleExports))
		return SCE_KERNEL_ERROR_MODULEMGR_INVALID_LIB;
	count = ((SceUInt32)end - (SceUInt32)ent) >> 5;
	if (count > 0xffff)
		return SCE_KERNEL_ERROR_MODULEMGR_INVALID_LIB;
	cb->libEntCount = count;

	while (ent < end) {
		if (ent->size != sizeof(SceModuleExports)) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 82, "illegal enttable element size %d byte\n",
					     ent->size);
			return SCE_KERNEL_ERROR_MODULEMGR_INVALID_LIB;
		}
		name = ent->libname;
		if (name != NULL) {
			if (!ModuleCBIsInReadOnly(cb, (SceUInt32)name, 1))
				return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
			if (!ModuleCBIsInReadOnly(cb, (SceUInt32)name,
						  strnlen(name + bias, 0x100)))
				return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
			if ((ent->attr & LIBRARY_ATTR_SYSCALL) != 0) {
				if (cb->pid != KERNEL_PID || ent->nvar != 0)
					return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
				cb->flags |= MODULE_FLAG_NO_STOP;
			}
		}
		ent = (SceModuleExports *)((SceUInt8 *)ent + ent->size);
	}
	if (cb->pid == KERNEL_PID || (cb->flags & MODULE_FLAG_SHARED_MASK) != 0) {
		libentSize = ctx->libentSize;
	} else {
		libentSize = count * sizeof(SceModuleExports);
		ctx->libentSize = libentSize;
	}
out:
	ctx->libEntCBSize = count * sizeof(LibEntCB);
	ctx->libstubOffset = ctx->linkInfoUnk + libentSize + count * sizeof(LibEntCB);
	return 0;
}

/* name unknown: one allocation for the copied tables and the control blocks */
int ModuleCBAllocLinkInfo(LoadelfContext *ctx)
{
	SceModuleCB *cb = ctx->cb;
	SceSize size;

	size = ctx->libentSize + ctx->linkInfoUnk + ctx->libEntCBSize + ctx->libstubSize +
	       ctx->libStubCBSize;
	if (size == 0)
		return 0;
	cb->linkInfo = ModulemgrAlloc(cb->pid, size);
	if (cb->linkInfo == NULL)
		return SCE_KERNEL_ERROR_MODULEMGR_NOMEM;
	memset(cb->linkInfo, 0, size);
	return 0;
}

int LibEntCBInit(LoadelfContext *ctx)
{
	SceModuleCB *cb = ctx->cb;
	SceUInt32 offset = ctx->linkInfoUnk;
	SceUInt16 count = cb->libEntCount;
	int local = (cb->flags & MODULE_FLAG_SHARED_MASK) == 0;
	SceModuleExports *src, *end, *dst, *exports;
	LibEntCB *ent;
	SceUInt32 bias, i;
	const char *name;
	char *interned;

	if (cb->pid == KERNEL_PID) {
		cb->libent = NULL;
	} else if (local) {
		bias = Loadelf_GetAddrBias(ctx, cb->libentTop);
		end = (SceModuleExports *)((SceUInt32)cb->libentBtm + bias);
		dst = (SceModuleExports *)((SceUInt8 *)cb->linkInfo + offset);
		offset += ctx->libentSize;
		cb->libent = dst;
		for (src = (SceModuleExports *)((SceUInt32)cb->libentTop + bias); src < end;
		     src = (SceModuleExports *)((SceUInt8 *)src + dst->size), dst++) {
			*dst = *src;
			if (dst->libname == NULL)
				continue;
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
		cb->libent = cb->shared->cb->libent;
	}

	if (ctx->libEntCBSize != 0) {
		ent = (LibEntCB *)((SceUInt8 *)cb->linkInfo + offset);
		cb->libEntCB = ent;
		memset(ent, 0, ctx->libEntCBSize);
		exports = cb->libent;
		if (exports == NULL)
			exports = cb->libentTop;
		ent = cb->libEntCB;
		for (i = 0; i < count; i++, ent++) {
			ent->cb = cb;
			ent->exports = &exports[i];
			if (!local)
				ent->flags = LIBENT_FLAG_SHARED;
		}
	}
	return 0;
}

/* name unknown: release the interned library names */
void LibEntFreeNames(SceModuleCB *cb)
{
	SceModuleExports *ent;
	int i, count;

	if ((cb->flags & MODULE_FLAG_SHARED_MASK) == 0) {
		ent = cb->libent;
		count = cb->libEntCount;
		if (ent != NULL && count != 0) {
			i = 0;
			do {
				if (ent->libname != NULL)
					SceSysmemForKernel_634D0D0C(ent->libname, 0x1000000);
				i++;
				ent++;
			} while (i < count);
		}
	}
}

/* name unknown */
void ModuleCBFreeLinkInfo(SceModuleCB *cb)
{
	if (cb->linkInfo != NULL)
		ModulemgrFree(cb->pid, cb->linkInfo);
}

/* name unknown: index of the export entry at addr */
int LibEntGetIndex(SceModuleCB *cb, SceUInt32 addr)
{
	if ((SceUInt32)cb->libentTop <= addr && addr < (SceUInt32)cb->libentBtm) {
		addr -= (SceUInt32)cb->libentTop;
		if ((addr & 0x1f) == 0)
			return addr >> 5;
	}
	return SCE_KERNEL_ERROR_MODULEMGR_INVALID_LIB;
}

/* name unknown */
void LibEntCBClear(LibEntCB *ent)
{
	if (ent->puid > 0) {
		kscePUIDClose(ent->cb->pid, ent->puid);
		ent->puid = 0;
	}
	if (ent->guid > 0) {
		ksceGUIDClose(ent->guid);
		ent->guid = 0;
	}
	ent->syscallIndex = 0;
	ent->clientCount = 0;
	ent->clients = NULL;
	ent->next = NULL;
	ent->older = NULL;
}

/* name unknown: the NID, entry and hash tables of a library, copied in for users */
int LibEntGetNidTable(LibEntCB *ent, LibEntNidTable *info)
{
	SceModuleCB *cb = ent->cb;
	SceUID pid = cb->pid;
	SceModuleExports *exports;
	SceUInt32 nfunc, nvar, n, i;
	SceSize funcHash, nidSize, size;
	SceNID *nidtable;
	void **entrytable;
	SceUInt8 *copy;
	int ret;

	memset(info, 0, sizeof(*info));
	exports = ent->exports;
	info->libEntCB = ent;
	nfunc = exports->nfunc;
	nvar = exports->nvar;
	n = nfunc + nvar;
	if (n == 0)
		return 0;
	nidtable = exports->nidtable;
	entrytable = exports->entrytable;
	info->funcHashBits = g_hashShift[exports->hashinfo & 0xf];
	funcHash = 4 << (30 - info->funcHashBits);
	info->nfunc = nfunc;
	info->nidtable = nidtable;
	info->entrytable = entrytable;
	info->nvar = nvar;
	info->varHashBits = g_hashShift[exports->hashinfo >> 4];

	if (cb->pid != KERNEL_PID) {
		nidSize = n * 4;
		size = nidSize + ((4 << (30 - info->varHashBits)) + funcHash) * 2;
		if ((ent->flags & LIBENT_FLAG_SHARED) == 0 &&
		    (!ModuleCBIsInReadOnly(cb, (SceUInt32)nidtable, size) ||
		     !ModuleCBIsInReadOnly(cb, (SceUInt32)entrytable, nidSize)))
			return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
		copy = ModulemgrAlloc(pid, size + nidSize);
		if (copy == NULL)
			return SCE_KERNEL_ERROR_MODULEMGR_NOMEM_LIB;
		info->copy = copy;
		info->nidtable = (SceNID *)copy;
		ret = ksceKernelCopyFromUserProc(pid, copy, ent->exports->nidtable, size);
		if (ret < 0)
			return ret;
		info->entrytable = (void **)(copy + size);
		ret = ksceKernelCopyFromUserProc(pid, copy + size, ent->exports->entrytable, nidSize);
		if (ret < 0)
			return ret;
		if ((ent->flags & LIBENT_FLAG_SHARED) == 0) {
			for (i = 0; i < n; i++) {
				if (!ModuleCBIsInRange(cb, (SceUInt32)info->entrytable[i], 1))
					return SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
			}
			ent->flags |= LIBENT_FLAG_SHARED;
		}
	}
	info->hashTable = info->nidtable + info->nvar + info->nfunc;
	info->varHashTable = (SceUInt8 *)info->hashTable + funcHash * 2;
	return 0;
}

/* name unknown */
void LibEntReleaseNidTable(LibEntNidTable *info)
{
	if (info->copy != NULL)
		ModulemgrFree(info->libEntCB->cb->pid, info->copy);
}

/* name unknown: call func for every function ('f') and variable ('v') entry */
int LibEntForEachEntry(LibEntCB *ent, int (*func)(SceNID nid, void *entry, int type, void *arg),
		       void *arg)
{
	LibEntNidTable info;
	SceUInt32 i, n;
	int ret;

	ret = LibEntGetNidTable(ent, &info);
	if (ret >= 0 && (n = info.nfunc + info.nvar) != 0) {
		for (i = 0; i < n; i++) {
			ret = func(info.nidtable[i], info.entrytable[i], i < info.nfunc ? 'f' : 'v',
				   arg);
			if (ret < 0)
				break;
		}
	}
	LibEntReleaseNidTable(&info);
	return ret;
}
