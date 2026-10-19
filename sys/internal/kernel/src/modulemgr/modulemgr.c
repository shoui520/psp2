/*
 * modulemgr.c - loading, starting, stopping and unloading modules
 */

#include "modulemgr_internal.h"
#include <modulemgr/modulemgr_debugger.h>

SceUID g_mutex;
SceClass *g_moduleClass;
ModulemgrPrivate *g_private;
SceClass *g_libraryClass;
SceClass *g_stubClass;
int g_threadStateSlot;

static int g_hostIoFailed;		/* opening a module failed with "no such device" */
static SceModuleCB *g_unlinkList[0xa0];	/* sceKernelUnlinkNormalSyscall */

static struct {
	SceUID thid;
	SceUID modid;
} g_unloadMySelf;

static int g_fwInfoValid;
static SceKernelSystemSwVersion g_fwInfo;

int ModulemgrConstructor(SceModuleObject *obj)
{
	memset(&obj->cb, 0, offsetof(SceModuleCB, reserved3));
	obj->cb.reserved3 = 0;
	return 0;
}

int ModulemgrLockLibDB(void)
{
	int ret;

	SceKernelSuspendForDriver_4DF40893(0);
	ret = sceKernelLockMutex(g_mutex, 1, NULL);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 164);
	return ret;
}

int ModulemgrUnlockLibDB(void)
{
	int ret;

	ret = sceKernelUnlockMutex(g_mutex, 1);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 175);
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

/* name unknown */
int ModulemgrLinkLibraries(SceModuleCB *cb, SceUInt32 flags,
				  const ModulemgrTitlePatch *patch)
{
	if ((cb->flags & MODULE_FLAG_SHARED_MASK) != 0 && !LibraryDBIsShared(cb->pid))
		return LoadcoreLinkLibraryEntries(cb, flags | 0x1000000, patch);
	return LoadcoreLinkLibraryEntries(cb, flags, patch);
}

/* name unknown */
int ModulemgrUnregisterLibraries(SceModuleCB *cb, SceUInt32 flags)
{
	int ret;

	if ((cb->flags & (MODULE_FLAG_SHARED_COPY | MODULE_FLAG_SHARED_ORIGINAL)) != 0)
		flags |= 0x30;
	ret = LoadcoreUnregisterModuleLibraries(cb, flags);
	SceCpuForKernel_803C84BF();
	return ret;
}

/* name unknown */
int ModulemgrReleaseClients(SceModuleCB *cb, SceUInt32 flags)
{
	if ((cb->flags & MODULE_FLAG_SHARED_MASK) != 0 && !LibraryDBIsShared(cb->pid))
		return LoadcoreUnlinkLibraries(cb, flags | 0x30);
	return LoadcoreUnlinkLibraries(cb, flags);
}

int ModulemgrCopyFromUser(SceModuleCB *cb, void *dst, const void *src, SceSize len)
{
	if (!ModuleCBIsInRange(cb, (SceUInt32)src, len))
		return SCE_KERNEL_ERROR_MODULEMGR_INVALID_PROC_PARAM;
	return sceKernelCopyFromUserProc(cb->pid, dst, src, len);
}

int ModulemgrDestructor(SceModuleObject *obj)
{
	SceModuleCB *cb = &obj->cb;
	SceUID pid = cb->pid;
	int ret;

	if ((cb->flags & MODULE_FLAG_SHARED_MASK) == 0) {
		if (pid != KERNEL_PID && cb->exidxCopy != NULL)
			sceKernelFreeRemoteToolHeap(pid, cb->exidxCopy);
		if (cb->path != NULL)
			ModulemgrFree(pid, cb->path);
		LibStubFree(cb);
		LibEntFreeNames(cb);
	}
	ModuleCBFreeLinkInfo(cb);
	LoadelfFreeImage(cb);
	if (cb->pid != KERNEL_PID) {
		ret = SceProcessmgrForKernel_F160B4D8(pid, (cb->flags & MODULE_FLAG_TYPE_MASK) == 1);
		if (ret < 0)
			SCE_DBG_ASSERT_FAIL(973);
	}
	return 0;
}

static inline void set_module_flags(SceModuleCB *cb, SceUInt32 flags)
{
	SceUInt16 mflags;

	if ((flags & LOAD_FLAG_PROCESS_IMAGE) != 0) {
		mflags = MODULE_FLAG_PROCESS_IMAGE;
		SCE_DBG_ASSERT_CHECK((flags & 0x80f1) == 0, 1072);
	} else {
		mflags = (flags & LOAD_FLAG_8000) != 0 ? MODULE_FLAG_1000 : 0;
		if ((flags & LOAD_FLAG_TYPE1) != 0)
			mflags |= 1;
		else if ((flags & LOAD_FLAG_TYPE2) != 0)
			mflags |= 2;
		if ((flags & LOAD_FLAG_SHARED) != 0) {
			if (cb->shared == NULL)
				mflags |= MODULE_FLAG_SHARED_ORIGINAL;
			else if (!sceKernelCheckDipsw(0xd2))
				mflags |= MODULE_FLAG_SHARED_COPY;
			else
				mflags |= MODULE_FLAG_SHARED_TOOL;
		}
	}
	cb->flags |= mflags;
}

static inline int set_caller_auth_info(SceModuleCB *cb, const char *path, SceSelfAuthInfo *auth,
				       SceUInt32 flags, SelfAuthRequest *info)
{
	SceUID pid = cb->pid;
	int ret;

	ret = sceKernelGetProcessSelfAuthInfo((flags & 0x4002) != 0 ? pid : sceKernelGetProcessId(),
					       &info->caller);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1022, "get SelfAuthInfo failed (0x%08x)\n", ret);
		return ret;
	}
	info->module.sharedSecret3[0] = 0x10;
	if ((flags & LOAD_FLAG_PROCESS_IMAGE) != 0) {
		info->module.klicensee[0] = auth->klicensee[0];
		info->selfType |= 0x10000;
		info->module.klicensee[1] = auth->klicensee[1];
		info->module.klicensee[2] = auth->klicensee[2];
		info->module.klicensee[3] = auth->klicensee[3];
	} else if ((flags & LOAD_FLAG_SELF_AUTH) != 0) {
		info->module.klicensee[0] = info->caller.klicensee[0];
		info->module.klicensee[1] = info->caller.klicensee[1];
		info->module.klicensee[2] = info->caller.klicensee[2];
		info->module.klicensee[3] = info->caller.klicensee[3];
	}
	ret = SceIofilemgrForDriver_9C220246((flags & 0x1004) != 0 ? pid : KERNEL_PID, path, 1, &info->mediaType);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1049, "GetMediaType failed (0x%08x)\n", ret);
		return ret;
	}
	if (pid != KERNEL_PID)
		info->selfType |= 1;
	return 0;
}

int ModulemgrLoadModule(SceModuleObject *obj, const char *path, SceUID fd, SceSelfAuthInfo *auth,
			SceUInt32 flags)
{
	SceModuleCB *cb = &obj->cb;
	SceUID pid = cb->pid;
	LoadelfContext ctx;
	SelfAuthRequest info;
	void *copy;
	SceSize size;
	SceUID puid;
	int i, ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.cb = cb;
	set_module_flags(cb, flags);

	ret = ModuleCBSetPath(cb, path, flags);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1136, "setting a filename (%s) failed (0x%08x)\n",
				     path, ret);
		goto fail;
	}

	memset(&info, 0, sizeof(info));
	if ((flags & LOAD_FLAG_NO_AUTH) == 0 && cb->shared == NULL) {
		ret = set_caller_auth_info(cb, path, auth, flags, &info);
		if (ret < 0)
			goto fail;
	}

	ret = LoadelfParsePhdrs(&ctx, fd, &info, flags);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1180, "load image failed (0x%08x)\n", ret);
		goto fail;
	}
	ret = ModuleCBCopyInfo(&ctx);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1189, "ModuleCBCopyInfo failed (0x%08x)\n", ret);
		goto fail;
	}
	ret = LibEnt_checkEntTable(&ctx);
	if (ret == 0)
		ret = LibStub_checkStubTable(&ctx);
	if (ret == 0)
		ret = ModuleCBAllocLinkInfo(&ctx);
	if (ret == 0)
		ret = LibEntCBInit(&ctx);
	if (ret == 0)
		ret = LibStubCBInit(&ctx);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1204, "Link Info failed (0x%08x)\n", ret);
		goto fail;
	}
	ret = LoadelfMapImage(&ctx);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1213, "map image failed (0x%08x)\n", ret);
		goto fail;
	}
	if (auth != NULL)
		memcpy(auth, &info.module, sizeof(*auth));

	if ((cb->attr & 2) != 0 &&
	    (ret = LibraryDBFindModuleByName(pid, cb->name)) != SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD) {
		ret = SCE_KERNEL_ERROR_MODULEMGR_EXIST;
		goto fail;
	}

	if ((cb->flags & MODULE_FLAG_SHARED_MASK) == 0) {
		ret = LoadcoreExportModuleEntries(cb);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 1240, "Can't export module entries, res:0x%08x\n",
					     ret);
			goto fail;
		}
		if ((flags & LOAD_FLAG_NO_ENTRY) != 0 &&
		    (cb->moduleStart != NULL || cb->moduleStop != NULL ||
		     cb->moduleExit != NULL || cb->moduleBootstart != NULL)) {
			SCE_DBG_ASSERT_FAIL(1251);
			ret = SCE_KERNEL_ERROR_MODULEMGR_NOEXEC;
			goto fail;
		}
	}

	if (cb->pid == KERNEL_PID) {
		cb->exidxCopy = cb->exidxTop;
	} else if (sceKernelCheckDipsw(0xd2) &&
		   (SceUInt32)cb->exidxTop < (SceUInt32)cb->exidxBtm) {
		if ((cb->flags & MODULE_FLAG_SHARED_MASK) == 0) {
			size = (SceUInt32)cb->exidxBtm - (SceUInt32)cb->exidxTop;
			copy = sceKernelAllocRemoteToolHeap(pid, size);
			if (copy != NULL) {
				if (sceKernelCopyFromUserProc(pid, copy, cb->exidxTop, size) < 0)
					sceKernelFreeRemoteToolHeap(pid, copy);
				else
					cb->exidxCopy = copy;
			}
		} else {
			cb->exidxCopy = cb->shared->cb->exidxCopy;
		}
	}

	for (i = 0; i < cb->nSegments; i++) {
		ret = sceGUIDSetName(cb->segment[i].memblk, cb->name);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 1276, "ModulemgrRenameUID failed (0x%08x)\n", ret);
			goto fail;
		}
	}

	if (cb->pid != KERNEL_PID) {
		puid = SceSysmemForDriver_BF209859(pid, cb->modid);
		if (puid < 0) {
			ret = 0;	/* Sony reports success here */
			goto fail;
		}
		cb->puid = puid;
	}
	ModuleCBSetStatus(cb, MODULE_STATUS_LOADED);
	LibraryDBRegisterModule(cb);
	return 0;

fail:
	sceGUIDClose(cb->modid);
	return ret;
}

int ModulemgrCallEntryOnUserThread(SceModuleCB *cb, SceModuleEntry entry,
				   const SceModuleThreadParam *param, SceSize args, void *argp,
				   int userArgs, int *status)
{
	SceUID pid = cb->pid;
	SceModuleThreadParam tp;
	SceKernelThreadOptParam opt, *optp;
	ModulemgrThreadState *state;
	char name[0x20];
	SceUInt32 stack, prio, attr, sdk;
	int cpu, ret;
	SceUID thid;

	stack = 0x4000;
	if (SceSblACMgrForKernel_sceSblACMgrIsGameProgram(pid) && (cb->flags & MODULE_FLAG_1000) == 0)
		stack = 0x40000;
	prio = 0x10000100;
	cpu = 0;
	if (param != NULL) {
		if (sceKernelCopyFromUserProc(pid, &tp, param, sizeof(tp)) >= 0 && tp.numParams == 4) {
			prio = tp.priority;
			stack = tp.stackSize;
			cpu = tp.cpuAffinityMask;
		}
	}
	snprintf(name, sizeof(name), "%s EntryCall", cb->name);

	if (SceThreadmgrForDriver_20C228E4()) {
		state = SceThreadmgrForDriver_3A72C6D8(g_threadStateSlot);
		if ((state->flags & THREAD_STATE_RETRY_WAIT) != 0) {
			thid = state->thid;
			state->flags &= ~THREAD_STATE_RETRY_WAIT;
			goto wait;
		}
	}

	ret = SceSysrootForDriver_9AF075D4(pid);
	if (ret < 0) {
		if (ret == 0x8002802d) {
			state = SceThreadmgrForDriver_3A72C6D8(g_threadStateSlot);
			state->thid = -1;
			state->modid = cb->modid;
			state->flags = THREAD_STATE_RETRY_START | 2;
		}
		return ret;
	}

	if ((cb->flags & MODULE_FLAG_TYPE_MASK) == 1) {
		opt.size = sizeof(opt);
		opt.attr = 6;
		opt.kStackMemType = 0;
		opt.uStackMemType = 0x0e20d060;
		opt.uTLSMemType = 0x0e20d060;
		opt.uStackMemid = 0;
		opt.handedExceptions = 0;
		optp = &opt;
		attr = 0x2008000;
	} else {
		optp = NULL;
		attr = 0x8000;
	}

	if (cb->pid != KERNEL_PID && (ModulemgrGetTPIDRURO() & 0x10000) == 0)
		goto raise;
	for (;;) {
		thid = sceKernelCreateRemoteThread(pid, name, entry, prio, stack, attr, cpu, optp);
		if (thid >= 0) {
			if (!userArgs)
				ret = sceKernelStartThread(thid, args, argp);
			else
				ret = SceThreadmgrForKernel_CBADD6B3(thid, args, argp);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 479, "start failed (0x%08x)\n", ret);
				sceKernelDeleteThread(thid);
				SceSysrootForDriver_1B3FA457(pid);
				return ret;
			}
			SceSysrootForDriver_1B3FA457(pid);
			goto wait;
		}
		if (cb->pid == KERNEL_PID)
			break;
		if (thid != 0x8002802f) {
			if ((thid & 0xbfffffff) == 0x8002450d && !SceSysrootForKernel_6050A467(pid) &&
			    SceSysrootForDriver_CAE820B2(pid, &sdk) == 0 && sdk >= 0x3100000) {
				SceSysrootForDriver_1B3FA457(pid);
				SceIntrmgrForDriver_C568F145(0x202);
				return thid;
			}
			break;
		}
raise:
		thid = SceProcessmgrForKernel_BC110987(pid, 0x10000);
		if (thid < 0) {
			SceSysrootForDriver_1B3FA457(pid);
			return thid;
		}
	}
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 467, "create failed (0x%08x)\n", thid);
	SceSysrootForDriver_1B3FA457(pid);
	return thid;

wait:
	ret = sceKernelWaitThreadEnd(thid, NULL, NULL);
	if (ret < 0) {
		if (ret == 0x8002802d) {
			state = SceThreadmgrForDriver_3A72C6D8(g_threadStateSlot);
			state->thid = thid;
			state->modid = cb->modid;
			state->flags = THREAD_STATE_RETRY_WAIT | 2;
		} else if (ret != 0x8002802c) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 500, "wait failed (0x%08x)\n", ret);
			sceKernelDeleteThread(thid);
		}
		return ret;
	}
	sceKernelGetThreadExitStatus(thid, status);
	sceKernelDeleteThread(thid);
	return 0;
}

/* check the static probes of a kernel module and call its module_start */
static inline int ModulemgrCallStartEntry(SceModuleCB *cb, SceSize args, void *argp,
					  SceUInt32 flags, int *status, SceUInt32 *usec)
{
	SceModuleProbesInfo *probesInfo;
	SceUInt32 start;
	void **probe;
	int n, ret;

	start = SceThreadmgrForDriver_47F6DE49();
	*status = 0;
	if (cb->pid == KERNEL_PID) {
		probesInfo = cb->probesInfo;
		if (probesInfo == NULL) {
			if (cb->probes != NULL)
				SCE_DBG_PRINTF_LEVEL(1, 0, 571,
					"WARNING: module %s exports static probes but not probes info\n",
					cb->path);
		} else if (cb->probes == NULL) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 567,
				"WARNING: module %s exports static probes info but not probes\n",
				cb->path);
		} else if (probesInfo->version != 1) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 532,
				"WARNING: ignoring static probes in module %s with incorrect version (%d != %d)\n",
				cb->path, probesInfo->version, 1);
			cb->probesInfo = NULL;
			cb->probes = NULL;
		} else if (probesInfo->count == 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 538, "WARNING: module %s exports zero probes\n",
					     cb->path);
			cb->probesInfo = NULL;
			cb->probes = NULL;
		} else {
			n = 0;
			for (probe = cb->probes; *probe != NULL; probe++)
				n++;
			if (probesInfo->count == n) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 557, "module %s defines %d static probes\n",
						     cb->path, probesInfo->count);
			} else {
				SCE_DBG_PRINTF_LEVEL(1, 0, 550,
					"WARNING: module %s static probe count mismatch (%d != %d)\n",
					cb->path, n, probesInfo->count);
				cb->probesInfo = NULL;
				cb->probes = NULL;
			}
		}
	}

	if (cb->moduleStart == NULL) {
		*usec = SceThreadmgrForDriver_47F6DE49() - start;
		return 0;
	}
	if (cb->pid == KERNEL_PID) {
		*status = cb->moduleStart(args, argp);
		ret = 0;
	} else {
		ret = ModulemgrCallEntryOnUserThread(cb, cb->moduleStart, cb->startThreadParam,
						     args, argp, (flags >> 27) & 1, status);
	}
	if (*status == SCE_KERNEL_START_NO_RESIDENT + 1) {	/* 2: SCE_KERNEL_START_FAILED */
		SceThreadmgrForDriver_47F6DE49();
		return SCE_KERNEL_ERROR_MODULEMGR_START_FAILED;
	}
	*usec = SceThreadmgrForDriver_47F6DE49() - start;
	return ret;
}

int ModulemgrStartModule(SceModuleCB *cb, SceSize args, void *argp, SceUInt32 flags, int *status,
			 const ModulemgrTitlePatch *patch)
{
	ModulemgrThreadState *state;
	SceUInt32 usec;
	SceUInt8 old;
	int ret;

	ModulemgrLockLibDB();
	if (SceThreadmgrForDriver_20C228E4()) {
		state = SceThreadmgrForDriver_3A72C6D8(g_threadStateSlot);
		if ((state->flags & THREAD_STATE_RETRY_START) != 0) {
			state->flags &= ~THREAD_STATE_RETRY_START;
			goto unlocked;
		}
		if ((state->flags & THREAD_STATE_RETRY_WAIT) != 0)
			goto unlocked;
	}

	old = cb->status;
	switch (old) {
	case MODULE_STATUS_NONE:
		ret = SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
		break;
	case MODULE_STATUS_LOADED:
	case MODULE_STATUS_START_SUSPENDED:
		ModuleCBSetStatus(cb, MODULE_STATUS_STARTING);
		if (old != MODULE_STATUS_START_SUSPENDED) {
			ret = ModulemgrLinkLibraries(cb, flags & 0x4000000, patch);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 1431,
					"Can't link libraries, res:0x%08x\n", ret);
				break;
			}
			ret = LoadcoreRegisterModuleLibraries(cb);
			SceCpuForKernel_803C84BF();
			if (ret < 0) {
				ModulemgrReleaseClients(cb, 0);
				break;
			}
		}
		goto unlocked;
	case MODULE_STATUS_STOPPED:
		ret = SCE_KERNEL_ERROR_MODULEMGR_STOPPED;
		break;
	default:
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1406, "Can't start a module, status:0x%08x\n", old);
		ret = SCE_KERNEL_ERROR_MODULEMGR_STARTED;
		break;
	}
	ModuleCBSetStatus(cb, old);
	ModulemgrUnlockLibDB();
	return ret;

unlocked:
	ModulemgrUnlockLibDB();
	if ((cb->flags & MODULE_FLAG_PROCESS_IMAGE) != 0)
		goto started;

	ret = ModulemgrCallStartEntry(cb, args, argp, flags, status, &usec);
	if (ret == SCE_KERNEL_ERROR_MODULEMGR_START_FAILED)
		goto failed;
	if (ret < 0) {
		if ((SceUInt32)ret - 0x8002802c < 2) {	/* stopped or suspended */
			ModuleCBSetStatus(cb, MODULE_STATUS_START_SUSPENDED);
			return ret;
		}
		goto failed;
	}
	if (*status != SCE_KERNEL_START_NO_RESIDENT) {
		ModuleCBShowStartMessage(cb, 0, usec);
started:
		ModuleCBSetStatus(cb, MODULE_STATUS_STARTED);
		return 0;
	}
	if ((cb->flags & MODULE_FLAG_NO_STOP) != 0 && sceKernelSysrootGetStatus() > 0xff)
		SCE_PANIC(1470);
	ModuleCBShowStartMessage(cb, 1, usec);
	ModulemgrLockLibDB();
	ModulemgrUnregisterLibraries(cb, 0x20);
	ModulemgrReleaseClients(cb, 0);
	ModulemgrUnlockLibDB();
	ModuleCBSetStatus(cb, MODULE_STATUS_STOPPED);
	return 0;

failed:
	if ((cb->flags & MODULE_FLAG_NO_STOP) != 0)
		SCE_PANIC(1491);
	ModulemgrLockLibDB();
	ModulemgrUnregisterLibraries(cb, 0x20);
	ModulemgrReleaseClients(cb, 0);
	ModulemgrUnlockLibDB();
	ModuleCBSetStatus(cb, MODULE_STATUS_LOADED);
	return ret;
}

int ModulemgrProcEventHandler(int event, int phase, void *param, void *common)
{
	SCE_DBG_PRINTF_LEVEL(2, 0xb, 3610, "evtype 0x%08x. phase 0x%08x, pParam %p, pCommon %p\n",
			     event, phase, param, common);
	if (event == 0x10)
		return SelfDecrypterSuspend(0x80000);
	if (event == 0x11)
		return SelfDecrypterSuspend(0xf);
	return 0;
}

/* name unknown */
int ModulemgrReferProcessObject(SceUID pid)
{
	return sceGUIDReferObjectWithClass(pid, SceProcessmgrForKernel_98AE4BC8(), NULL);
}

/* name unknown */
int ModulemgrStrncpyFromUser(SceModuleCB *cb, char *dst, const char *src)
{
	if (ModuleCBIsInRange(cb, (SceUInt32)src, 1) &&
	    sceKernelStrncpyFromUserProc(cb->pid, dst, src, 0x1f) >= 0) {
		dst[0x1f] = '\0';
		return 0;
	}
	return SCE_KERNEL_ERROR_MODULEMGR_INVALID_PROC_PARAM;
}

SceModuleObject *ModulemgrReferModule(SceUID modid)
{
	SceModuleObject *obj;

	if (sceGUIDReferObjectWithClass(modid, g_moduleClass, (SceObjectBase **)&obj) < 0)
		obj = NULL;
	return obj;
}

/* the function NID of the stub at addr */
static inline int ModulemgrFindStubNid(LibStubCB *stub, SceUInt32 addr, SceNID *nid)
{
	SceUInt32 n = stub->imports->nfunc;
	SceUInt32 i;

	if (n == 0)
		return -1;
	for (i = 0; stub->toolCopy[i] != (void *)addr; i++) {
		if (i + 1 >= n)
			return -1;
	}
	*nid = (SceNID)stub->toolCopy[i + n];
	return 0;
}

void ShowCallerInfo(SceUID pid, SceUInt32 addr)
{
	SceModuleObject *obj;
	LibStubCB *stub;
	SceUID modid;
	SceNID nid;
	int seg = -1, found = -1;
	SceUInt32 offset = -1;

	if (!sceKernelCheckDipsw(0xd2))
		return;
	modid = LibraryDBGetModuleUIDByAddr(pid, addr);
	if (modid < 0)
		return;
	obj = ModulemgrReferModule(modid);
	if (obj == NULL)
		return;
	stub = LibraryDBGetStubCBByStubCodeAddrForTool(&obj->cb, addr);
	if (stub != NULL && ModuleCBGetSegmentOffset(&obj->cb, addr, &seg, &offset) == 0) {
		found = ModulemgrFindStubNid(stub, addr, &nid);
		SCE_PRINTF_WITH_INFO(0, 3930,
			"\n=== Caller Module Info (that caused PABT) ==============\n");
		SCE_PRINTF_WITH_INFO(0, 3931, "Module Name    : %s\n", obj->cb.name);
		SCE_PRINTF_WITH_INFO(0, 3932, "Path           : %s\n", obj->cb.path);
		SCE_PRINTF_WITH_INFO(0, 3933, "Stub Code Addr : 0x%08x (seg:offset %d:0x%08x)\n",
				     addr, seg, offset);
		SCE_PRINTF_WITH_INFO(0, 3934, "Library Name   : %s (ver:%d)\n",
				     stub->imports->libname, stub->imports->version);
		if (found == 0)
			SCE_PRINTF_WITH_INFO(0, 3936, "Function NID   : 0x%08x\n", nid);
		else
			SCE_PRINTF_WITH_INFO(0, 3938, "Function NID   : ????\n");
		SCE_PRINTF_WITH_INFO(0, 3940,
			"========================================================\n\n");
	}
	sceGUIDReleaseObject(modid);
}

/* name unknown: the library function a stub calls (sysroot callback) */
int ModulemgrGetStubInfo(SceUID pid, SceUInt32 addr, ModulemgrStubInfo *info)
{
	SceModuleObject *obj;
	LibStubCB *stub;
	SceUID modid;
	SceUInt32 i, n;
	SceNID nid;

	if (info == NULL || info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!sceKernelCheckDipsw(0xd2) || (modid = LibraryDBGetModuleUIDByAddr(pid, addr)) < 0 ||
	    (obj = ModulemgrReferModule(modid)) == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD;
	stub = LibraryDBGetStubCBByStubCodeAddrForTool(&obj->cb, addr);
	if (stub == NULL || (n = stub->imports->nfunc) == 0)
		goto nolib;
	for (i = 0; stub->toolCopy[i] != (void *)addr; i++) {
		if (i + 1 >= n)
			goto nolib;
	}
	nid = (SceNID)stub->toolCopy[i + n];
	info->modid = modid;
	info->attr = obj->cb.attr;
	info->version[0] = obj->cb.version[0];
	info->version[1] = obj->cb.version[1];
	strncpy(info->moduleName, obj->cb.name, sizeof(info->moduleName));
	info->fingerprint = obj->cb.fingerprint;
	info->libname = stub->linked->exports->libname;
	info->reserved0 = 0;
	info->libVersion = stub->imports->version;
	info->funcnid = nid;
	sceGUIDReleaseObject(modid);
	return 0;
nolib:
	sceGUIDReleaseObject(modid);
	return SCE_KERNEL_ERROR_LIBRARYDB_NO_LIB;
}

void ModulemgrReleaseModule(SceUID modid)
{
	sceGUIDReleaseObject(modid);
}

int ModulemgrReferProcess(SceUID pid)
{
	if (pid != KERNEL_PID)
		return ModulemgrReferProcessObject(pid);
	return 0;
}

int ModulemgrReleaseProcess(SceUID pid)
{
	if (pid != KERNEL_PID)
		sceGUIDReleaseObject(pid);
	return 0;
}

int loadModuleCommon(SceUID pid, const char *path, SceUInt32 flags,
			    const SceKernelLoadModuleOpt *opt)
{
	ModulemgrThreadState *state;
	SharedModuleEntry *shared;
	SceModuleObject *obj;
	LibraryDB *db;
	SceUInt64 time;
	SceUInt32 dbgFlags, inhibit;
	SceUInt32 t0, t1;
	SceUID ret, fd, modid;
	int intr;

	if (opt != NULL && opt->size != 4)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (pid == 0)
		pid = sceKernelGetProcessId();
	fd = flags & LOAD_FLAG_SHARED;
	if (fd != 0) {
		if (pid == KERNEL_PID)
			return SCE_KERNEL_ERROR_MODULEMGR_BAD_PID;
		if (ModulemgrReferProcessObject(pid) < 0)
			return SCE_KERNEL_ERROR_MODULEMGR_NO_PROCESS;
	} else if (pid != KERNEL_PID) {
		if (ModulemgrReferProcessObject(pid) < 0)
			return SCE_KERNEL_ERROR_MODULEMGR_NO_PROCESS;
	}

	if (SceThreadmgrForDriver_20C228E4()) {
		state = SceThreadmgrForDriver_3A72C6D8(g_threadStateSlot);
		if ((state->flags & THREAD_STATE_LOAD_RESUMED) != 0) {
			modid = state->modid;
			time = state->time;
			state->flags &= ~THREAD_STATE_LOAD_RESUMED;
			goto notify;
		}
		if ((state->flags & THREAD_STATE_LOAD_SUSPENDED) != 0) {
			ret = state->modid;
			state->flags &= ~THREAD_STATE_LOAD_SUSPENDED;
			goto out;
		}
	}

	if (strncmp(path, "host0:", 6) == 0) {
		if (pid == KERNEL_PID && !SceQafMgrForDriver_082A4FC2())
			return SCE_KERNEL_ERROR_MODULEMGR_NOENT;
		if (fd != 0 && !sceKernelCheckDipsw(0xfb)) {
			ret = SCE_KERNEL_ERROR_MODULEMGR_NOENT;
			goto out;
		}
	}

	db = LibraryDBLock(pid, &intr);
	if (db == NULL) {
		ret = SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
		goto out;
	}
	inhibit = db->flags;
	LibraryDBUnlock(db, intr);
	switch (inhibit & LIBDB_FLAG_INHIBIT_MASK) {
	case 0x10:
		if ((flags & LOAD_FLAG_8000) == 0)
			goto inhibited;
		break;
	case 0x20:
		if ((flags & LOAD_FLAG_TYPE1) == 0)
			goto inhibited;
		break;
	case 0x30:
inhibited:
		ret = SCE_KERNEL_ERROR_MODULEMGR_INHIBITED;
		goto out;
	}

	if (fd != 0) {
		if (LibraryDBIsShared(pid)) {
			flags = (flags & ~0xf0u) | LOAD_FLAG_TYPE2;
		} else if ((shared = SharedModuleFind(path)) != NULL) {
			ret = ModuleObjNew(pid, flags, &obj);
			if (ret < 0) {
				SceKernelSuspendForDriver_2BB92967(0);
				SharedModuleRelease(shared);
				goto out;
			}
			obj->cb.shared = shared;
			t0 = SceThreadmgrForDriver_47F6DE49();
			ret = ModulemgrLoadModule(obj, path, -1, NULL, flags);
			goto loaded;
		}
	}

	SceKernelSuspendForDriver_4DF40893(0);
	if ((flags & LOAD_FLAG_1000) != 0)
		fd = SceIofilemgrForDriver_C3D34965(pid, path, SCE_O_RDONLY, 0);
	else if ((flags & LOAD_FLAG_NO_AUTH) != 0)
		fd = BootimageOpen(path);
	else
		fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0) {
		if ((fd & 0xfffeffff) == 0x8008000a)
			g_hostIoFailed = 1;
		SceKernelSuspendForDriver_2BB92967(0);
		if (pid != KERNEL_PID)
			sceGUIDReleaseObject(pid);
		return fd;
	}
	ret = ModuleObjNew(pid, flags, &obj);
	if (ret < 0) {
		if (fd != 0) {
			if ((flags & LOAD_FLAG_NO_AUTH) != 0)
				sceLoadcoreIoClose(fd);
			else
				sceIoClose(fd);
		}
		SceKernelSuspendForDriver_2BB92967(0);
		goto out;
	}
	obj->cb.shared = NULL;
	t0 = SceThreadmgrForDriver_47F6DE49();
	ret = ModulemgrLoadModule(obj, path, fd, NULL, flags);
	if (fd > 0) {
		if ((flags & LOAD_FLAG_NO_AUTH) != 0)
			sceLoadcoreIoClose(fd);
		else
			sceIoClose(fd);
	}

loaded:
	SceKernelSuspendForDriver_2BB92967(0);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 2206, "load (%s) failed (0x%08x)\n", path, ret);
		if (pid != KERNEL_PID)
			sceGUIDReleaseObject(pid);
		return ret;
	}
	SceCpuForKernel_803C84BF();
	t1 = SceThreadmgrForDriver_47F6DE49();
	SCE_DBG_PRINTF_LEVEL(1, 0, 2219,
		"Loading %s (guid=0x%08x, fingerprint=0x%08x): OK.   %d usec (form 0x%03x)\n",
		obj->cb.path, obj->cb.modid, obj->cb.fingerprint, t1 - t0, g_relocForms);
	(void)t0;
	(void)t1;
	relocatorFormCount();
	ModuleCBDumpLoadAddress(&obj->cb);

	dbgFlags = 0;
	modid = obj->cb.modid;
	ret = modid;
	SceSysrootForKernel_73522F65(pid, &dbgFlags);
	time = sceKernelGetSystemTimeWide();
	if (pid != KERNEL_PID) {
		if ((flags & 0x40000) != 0) {
			dbgFlags = 0;
			SceSysrootForKernel_73522F65(pid, &dbgFlags);
			goto out;
		}
		if (SceSysrootForKernel_6050A467(pid) == 1 || (flags & 0x20000) != 0)
			SceSysrootForKernel_C81B7E2B(pid, modid, time);
		dbgFlags = 0;
		SceSysrootForKernel_73522F65(pid, &dbgFlags);
		ret = modid;
		goto report;
	}
notify:
	dbgFlags = 0;
	SceSysrootForKernel_73522F65(pid, &dbgFlags);
	ret = modid;
	if ((flags & 0x40000) != 0)
		goto out;
report:
	if ((dbgFlags & 1) != 0) {
		state = SceThreadmgrForDriver_3A72C6D8(g_threadStateSlot);
		intr = SceSysrootForKernel_256B2394(pid, modid, 0x20006, time);
		if (intr < 0) {
			if (intr == 0x8002802d) {
				state->modid = modid;
				state->time = time;
				state->flags = THREAD_STATE_LOAD_RESUMED;
				ret = 0x8002802d;
			} else if (intr != 0x8002802c) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 2259,
					"sceKernelSysrootDbgpSuspendProcessAndWaitResume : failed (0x%08x)\n",
					intr);
			}
		} else {
			state->modid = modid;
			state->flags = THREAD_STATE_LOAD_SUSPENDED;
		}
	} else {
		SceSysrootForKernel_CA497324(pid, modid, time);
	}
out:
	if (pid != KERNEL_PID)
		sceGUIDReleaseObject(pid);
	return ret;
}

SceUID sceKernelLoadModule(const char *path, SceUInt32 flags, const SceKernelLoadModuleOpt *opt)
{
	if ((flags & 0xfff8260f) != 0)
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	return loadModuleCommon(KERNEL_PID, path, flags, opt);
}

SceUID SceModulemgrForKernel_4E85022D(SceUID pid, const char *path, SceUInt32 flags,
				  const SceKernelLoadModuleOpt *opt)
{
	int perm;
	SceUID old;
	SceUID ret;

	if (pid == 0)
		return SCE_KERNEL_ERROR_MODULEMGR_BAD_PID;
	if ((flags & 0xfff8260f) != 0)
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	perm = sceKernelSetThreadAccessLevel(0x80);
	old = SceThreadmgrForDriver_0486F239(KERNEL_PID);
	ret = loadModuleCommon(pid, path, flags | LOAD_FLAG_USE_PID, opt);
	SceThreadmgrForDriver_0486F239(old);
	sceKernelSetThreadAccessLevel(perm);
	return ret;
}

/* name unknown */
SceUID ModulemgrLoadSharedModuleForPid(SceUID pid, const char *path, SceUInt32 flags)
{
	int perm;
	SceUID old;
	SceUID ret;

	if (pid == 0)
		return SCE_KERNEL_ERROR_MODULEMGR_BAD_PID;
	perm = sceKernelSetThreadAccessLevel(0x80);
	old = SceThreadmgrForDriver_0486F239(KERNEL_PID);
	ret = loadModuleCommon(pid, path, flags | LOAD_FLAG_SHARED, NULL);
	SceThreadmgrForDriver_0486F239(old);
	sceKernelSetThreadAccessLevel(perm);
	return ret;
}

static inline int ModulemgrUnloadModule(SceUID pid, SceModuleObject *obj, SceUID modid,
					SceUInt32 flags)
{
	SceModuleCB *cb = &obj->cb;
	SceUInt64 time;
	SceUID puid;

	ModulemgrLockLibDB();
	if (cb->status != MODULE_STATUS_LOADED && cb->status != MODULE_STATUS_STOPPED) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1330, "Can't unload a module, status:0x%02x\n",
				     cb->status);
		ModulemgrUnlockLibDB();
		return SCE_KERNEL_ERROR_MODULEMGR_NOT_STOPPED;
	}
	ModuleCBSetStatus(cb, MODULE_STATUS_NONE);
	ModulemgrUnlockLibDB();
	puid = cb->puid;
	if (puid > 0) {
		SceSysmemForDriver_84A4AF5E(pid, puid);
		cb->puid = 0;
	}
	if (cb->pid != KERNEL_PID && (flags & 0x40000000) == 0 &&
	    (SceSysrootForKernel_6050A467(pid) == 1 || (flags & 0x20000) != 0)) {
		time = sceKernelGetSystemTimeWide();
		SceSysrootForKernel_20D2A0DF(pid, puid, time);
		SceSysrootForKernel_AA8D34EE(pid, puid, time);
	}
	LibraryDBReleaseModule(cb);
	sceGUIDClose(modid);
	return 0;
}

int unloadModuleCommon(SceUID pid, SceUID modid, SceUInt32 flags,
			      const SceKernelUnloadModuleOpt *opt)
{
	SceModuleObject *obj;
	int ret;

	if (opt != NULL && opt->size != 4)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (pid == 0)
		pid = sceKernelGetProcessId();
	if (pid == KERNEL_PID) {
		obj = ModulemgrReferModule(modid);
		if (obj == NULL)
			return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
	} else {
		if (ModulemgrReferProcessObject(pid) < 0)
			return SCE_KERNEL_ERROR_MODULEMGR_NO_PROCESS;
		obj = ModulemgrReferModule(modid);
		if (obj == NULL) {
			sceGUIDReleaseObject(pid);
			return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
		}
	}
	if (pid != obj->cb.pid) {
		sceGUIDReleaseObject(modid);
		if (pid != KERNEL_PID)
			sceGUIDReleaseObject(pid);
		return SCE_KERNEL_ERROR_MODULEMGR_BAD_PID;
	}
	ret = ModulemgrUnloadModule(pid, obj, modid, flags);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 2536, "unload failed (res:0x%08x)\n", ret);
	sceGUIDReleaseObject(modid);
	if (pid != KERNEL_PID)
		sceGUIDReleaseObject(pid);
	SceCpuForKernel_803C84BF();
	return ret;
}

int sceKernelUnloadModule(SceUID modid, SceUInt32 flags, const SceKernelUnloadModuleOpt *opt)
{
	if ((flags & 0xbfffffff) != 0)
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	return unloadModuleCommon(KERNEL_PID, modid, flags, opt);
}

int SceModulemgrForKernel_FCA9FDB1(SceUID pid, SceUID modid, SceUInt32 flags,
				 const SceKernelUnloadModuleOpt *opt)
{
	if (pid == 0)
		return SCE_KERNEL_ERROR_MODULEMGR_BAD_PID;
	if ((flags & 0xbfffffff) != 0)
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	return unloadModuleCommon(pid, modid, flags, opt);
}

/* tell the debugger the start failed */
static inline void sendDbgpStartupFailed(SceUID pid, SceUID modid, int result)
{
	ModulemgrThreadState *state;
	SceUInt64 time;
	SceUInt32 dbgFlags = 0;
	int ret;

	if ((SceUInt32)result - 0x8002802c < 2)
		return;
	SceSysrootForKernel_73522F65(pid, &dbgFlags);
	if ((dbgFlags & 1) == 0 || pid != sceKernelGetProcessId())
		return;
	state = SceThreadmgrForDriver_3A72C6D8(g_threadStateSlot);
	time = sceKernelGetSystemTimeWide();
	ret = SceSysrootForKernel_256B2394(pid, modid, 0x20005, time);
	if (ret < 0) {
		if (ret == 0x8002802d) {
			state->modid = modid;
			state->time = time;
			state->flags = THREAD_STATE_START_RESULT | THREAD_STATE_START_SUSPEND;
			state->result = result;
		}
		SCE_DBG_PRINTF_LEVEL(1, 0, 2689,
			"sceKernelSysrootDbgpSuspendProcessAndWaitResume : failed (0x%08x)\n", ret);
	} else {
		state->flags = THREAD_STATE_START_RESULT;
		state->result = result;
	}
}

/* the same for a start syscall that is retried */
static inline void sendDbgpStartupFailedForRetryedSyscall(SceUID pid, ModulemgrThreadState *retry)
{
	ModulemgrThreadState *state;
	SceUID modid = retry->modid;
	SceUInt64 time = retry->time;
	int result = retry->result;
	SceUInt32 dbgFlags = 0;
	int ret;

	SceSysrootForKernel_73522F65(pid, &dbgFlags);
	if ((dbgFlags & 1) == 0 || pid != sceKernelGetProcessId())
		return;
	state = SceThreadmgrForDriver_3A72C6D8(g_threadStateSlot);
	ret = SceSysrootForKernel_256B2394(pid, modid, 0x20005, time);
	if (ret < 0) {
		if (ret == 0x8002802d) {
			state->modid = modid;
			state->time = time;
			state->flags = THREAD_STATE_START_RESULT | THREAD_STATE_START_SUSPEND;
			state->result = result;
		}
		SCE_DBG_PRINTF_LEVEL(1, 0, 2728,
			"sceKernelSysrootDbgpSuspendProcessAndWaitResume : failed (0x%08x)\n", ret);
	} else {
		state->flags = THREAD_STATE_START_RESULT;
		state->result = result;
	}
}

int startModuleCommon(SceUID pid, SceUID modid, SceSize args, void *argp, SceUInt32 flags,
			     const SceKernelStartModuleOpt *opt, int *status)
{
	ModulemgrThreadState *state;
	SceModuleObject *obj;
	int report, ret, st = 0;

	if (opt != NULL && opt->size != 0x10) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		goto failed;
	}
	if (SceThreadmgrForDriver_20C228E4()) {
		state = SceThreadmgrForDriver_3A72C6D8(g_threadStateSlot);
		if ((state->flags & THREAD_STATE_START_RESULT) != 0) {
			if ((state->flags & THREAD_STATE_START_SUSPEND) != 0)
				sendDbgpStartupFailedForRetryedSyscall(pid, state);
			return state->result;
		}
	}
	if (pid == 0)
		pid = sceKernelGetProcessId();
	if (pid != KERNEL_PID && ModulemgrReferProcessObject(pid) < 0) {
		ret = SCE_KERNEL_ERROR_MODULEMGR_NO_PROCESS;
		goto failed;
	}

	obj = ModulemgrReferModule(modid);
	if (obj == NULL) {
		report = 1;
		ret = SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
	} else if (pid != obj->cb.pid) {
		report = 1;
		ret = SCE_KERNEL_ERROR_MODULEMGR_BAD_PID;
		sceGUIDReleaseObject(modid);
	} else {
		ret = ModulemgrStartModule(&obj->cb, args, argp, flags, &st, NULL);
		sceGUIDReleaseObject(modid);
		if (st == SCE_KERNEL_START_NO_RESIDENT ||
		    (st == 2 && (obj->cb.flags & MODULE_FLAG_SHARED_ANY) != 0)) {
			unloadModuleCommon(pid, modid, 0, NULL);
			if (st != 2)
				goto ok;
			report = ret != 0x8002802f && ret != 0x8002802c;
		} else if (ret >= 0) {
ok:
			if (pid != KERNEL_PID)
				sceGUIDReleaseObject(pid);
			if (status != NULL)
				*status = st;
			return 0;
		} else if (ret == 0x8002802d) {
			report = 1;
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2810, "suspended  (res:0x%08x)\n", 0x8002802d);
		} else if (ret == 0x8002802c) {
			report = 0;
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2812, "stopped (res:0x%08x)\n", 0x8002802c);
		} else {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 2814, "start failed (res:0x%08x)\n", ret);
			report = ret != 0x8002802f;
		}
	}
	if (pid != KERNEL_PID)
		sceGUIDReleaseObject(pid);
	if (!report || ret == SCE_KERNEL_ERROR_MODULEMGR_STOPPED)
		return ret;
failed:
	sendDbgpStartupFailed(pid, modid, ret);
	return ret;
}

int sceKernelStartModule(SceUID modid, SceSize args, void *argp, SceUInt32 flags,
			  const SceKernelStartModuleOpt *opt, int *status)
{
	if (flags != 0)
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	return startModuleCommon(KERNEL_PID, modid, args, argp, 0, opt, status);
}

int SceModulemgrForKernel_3FE47DDF(SceUID pid, SceUID modid, SceSize args, void *argp,
				SceUInt32 flags, const SceKernelStartModuleOpt *opt, int *status)
{
	if (pid == 0)
		return SCE_KERNEL_ERROR_MODULEMGR_BAD_PID;
	if (flags != 0)
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	return startModuleCommon(pid, modid, args, argp, flags, opt, status);
}

static inline int ModulemgrStopModule(SceModuleObject *obj, SceSize args, void *argp,
				      SceUInt32 flags, int *status)
{
	SceModuleCB *cb = &obj->cb;
	ModulemgrThreadState *state;
	SceUInt8 old;
	int ret;

	ModulemgrLockLibDB();
	old = cb->status;
	if ((flags & 0x200) != 0) {
		if (old == MODULE_STATUS_LOADED) {
			ret = (flags & 0x100) != 0 ? 0 : SCE_KERNEL_ERROR_MODULEMGR_NOT_STARTED;
		} else if (old == MODULE_STATUS_STOPPED) {
			ret = (flags & 0x100) != 0 ? 0 : SCE_KERNEL_ERROR_MODULEMGR_STOPPED;
		} else if ((flags & 0x30) != 0) {
			ModuleCBSetStatus(cb, MODULE_STATUS_STOPPING);
			goto unlink;
		} else {
			ret = SCE_KERNEL_ERROR_MODULEMGR_NOT_STARTED;
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 1588,
				"Can't stop a module, status:%d, flags 0x%08x\n", old, flags);
		}
		goto restore;
	}

	if (SceThreadmgrForDriver_20C228E4()) {
		state = SceThreadmgrForDriver_3A72C6D8(g_threadStateSlot);
		if ((state->flags & THREAD_STATE_RETRY_START) != 0) {
			state->flags &= ~THREAD_STATE_RETRY_START;
			goto call;
		}
		if ((state->flags & THREAD_STATE_RETRY_WAIT) != 0)
			goto call;
	}
	switch (old) {
	case MODULE_STATUS_LOADED:
		ret = SCE_KERNEL_ERROR_MODULEMGR_NOT_STARTED;
		break;
	case MODULE_STATUS_STARTED:
	case MODULE_STATUS_START_SUSPENDED:
		ModuleCBSetStatus(cb, MODULE_STATUS_STOPPING);
		goto call;
	case MODULE_STATUS_STOPPED:
		ret = SCE_KERNEL_ERROR_MODULEMGR_STOPPED;
		break;
	default:
		ret = SCE_KERNEL_ERROR_MODULEMGR_NOT_STARTED;
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1648, "Can't stop a module, status:%d\n", old);
		break;
	}
	goto restore;

call:
	ModulemgrUnlockLibDB();
	*status = 0;
	if (cb->moduleStop != NULL) {
		if (cb->pid == KERNEL_PID) {
			*status = cb->moduleStop(args, argp);
			if (*status == SCE_KERNEL_STOP_CANCEL)
				goto cancel;
		} else {
			ret = ModulemgrCallEntryOnUserThread(cb, cb->moduleStop, cb->stopThreadParam,
							     args, argp, flags >> 27, status);
			if (*status == SCE_KERNEL_STOP_CANCEL)
				goto cancel;
			if (ret != 0) {
				if ((SceUInt32)ret - 0x8002802c < 2)
					return ret;
				goto started;
			}
		}
	}
	ModulemgrLockLibDB();
unlink:
	ret = ModulemgrReleaseClients(cb, flags);
	if (ret >= 0) {
		ret = ModulemgrUnregisterLibraries(cb, flags);
		if (ret < 0) {
			old = MODULE_STATUS_STARTED;
			ModulemgrLinkLibraries(cb, 0, NULL);
		} else {
			old = MODULE_STATUS_STOPPED;
		}
	}
restore:
	ModuleCBSetStatus(cb, old);
	ModulemgrUnlockLibDB();
	return ret;

cancel:
	ret = SCE_KERNEL_ERROR_MODULEMGR_STOP_FAIL;
started:
	ModuleCBSetStatus(cb, MODULE_STATUS_STARTED);
	return ret;
}

int stopModuleCommon(SceUID pid, SceUID modid, SceSize args, void *argp, SceUInt32 flags,
			    const SceKernelStopModuleOpt *opt, int *status)
{
	SceModuleObject *obj;
	SceUInt32 mflags;
	int ret, st = 0;

	if (opt != NULL && opt->size != 0x10)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (pid == 0)
		pid = sceKernelGetProcessId();
	if (pid == KERNEL_PID) {
		obj = ModulemgrReferModule(modid);
		if (obj == NULL)
			return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
	} else {
		if (ModulemgrReferProcessObject(pid) < 0)
			return SCE_KERNEL_ERROR_MODULEMGR_NO_PROCESS;
		obj = ModulemgrReferModule(modid);
		if (obj == NULL) {
			sceGUIDReleaseObject(pid);
			return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
		}
	}
	if (pid != obj->cb.pid) {
		sceGUIDReleaseObject(modid);
		if (pid != KERNEL_PID)
			sceGUIDReleaseObject(pid);
		return SCE_KERNEL_ERROR_MODULEMGR_BAD_PID;
	}
	if ((flags & 0x100) == 0) {
		if ((obj->cb.attr & 1) != 0 || (obj->cb.flags & MODULE_FLAG_SHARED_ORIGINAL) != 0) {
			sceGUIDReleaseObject(modid);
			if (pid != KERNEL_PID)
				sceGUIDReleaseObject(pid);
			return SCE_KERNEL_ERROR_MODULEMGR_CANNOT_STOP;
		}
	}
	mflags = obj->cb.flags;
	if ((mflags & MODULE_FLAG_NO_STOP) != 0 && !SceSblACMgrForKernel_BBA13D9C()) {
		sceGUIDReleaseObject(modid);
		if (pid != KERNEL_PID)
			sceGUIDReleaseObject(pid);
		return SCE_KERNEL_ERROR_MODULEMGR_IN_USE;
	}
	mflags = flags & 0x8000300;
	if ((flags & 0x30) != 0)
		mflags |= 0x30;

	ret = ModulemgrStopModule(obj, args, argp, mflags, &st);
	sceGUIDReleaseObject(modid);
	if (pid != KERNEL_PID)
		sceGUIDReleaseObject(pid);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 2952, "stop failed (res:0x%08x)\n", ret);
		return ret;
	}
	if (status != NULL)
		*status = st;
	return 0;
}

int StopUnloadModuleCommon(SceUID pid, SceUID modid, SceSize args, void *argp, SceUInt32 flags,
			   const SceKernelUnloadModuleOpt *opt, int *status)
{
	int ret;
#if FW_DEBUG
	SceKernelModuleInfo info;
	const char *name = info.module_name;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	if (SceModulemgrForKernel_DAA90093(pid, modid, &info) != 0)
		name = "invalid";
#endif

	ret = stopModuleCommon(pid, modid, args, argp, flags, NULL, status);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2604,
			"Stopping %s module (pid=0x%08x, guid=0x%08x): Failed (0x%08x)\n",
			name, pid, modid, ret);
		return ret;
	}
	ret = unloadModuleCommon(pid, modid, flags, opt);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2613,
			"Unloading %s module (pid=0x%08x, guid=0x%08x): Failed (0x%08x)\n",
			name, pid, modid, ret);
	else
		SCE_DBG_PRINTF_LEVEL(1, 0, 2616,
			"Unloading %s module (pid=0x%08x, guid=0x%08x): OK\n", name, pid, modid);
	return ret;
}

/* name unknown */
SceUID loadStartModuleCommon(SceUID pid, const char *path, SceSize args, void *argp,
				    SceUInt32 flags, const SceKernelLoadModuleOpt *opt, int *status)
{
	SceUID modid;
	int ret;

	modid = loadModuleCommon(pid, path, flags, opt);
	if (modid < 0)
		return modid;
	ret = startModuleCommon(pid, modid, args, argp, flags, NULL, status);
	if (ret == 0)
		return modid;
	if (ret == 1)
		return 0;
	if ((SceUInt32)ret - 0x8002802c >= 2)
		StopUnloadModuleCommon(pid, modid, 0, NULL, 0x40000000, NULL, NULL);
	return ret;
}

SceUID SceModulemgrForDriver_189BFBBB(const char *path, SceSize args, void *argp, SceUInt32 flags,
				 const SceKernelLoadModuleOpt *opt, int *status)
{
	if ((flags & 0xfff8260f) != 0)
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	return loadStartModuleCommon(KERNEL_PID, path, args, argp, flags, opt, status);
}

SceUID SceModulemgrForDriver_9D953C22(SceUID pid, const char *path, SceSize args, void *argp,
				       SceUInt32 flags, const SceKernelLoadModuleOpt *opt, int *status)
{
	if (pid == 0)
		return SCE_KERNEL_ERROR_MODULEMGR_BAD_PID;
	if ((flags & 0xfff8260f) != 0)
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	return loadStartModuleCommon(pid, path, args, argp, flags | LOAD_FLAG_FOR_PID | LOAD_FLAG_USE_PID,
				     opt, status);
}

int SceModulemgrForDriver_02D3D0C1(SceUID pid, SceUID modid, SceSize args, void *argp,
					   SceUInt32 flags, const SceKernelUnloadModuleOpt *opt,
					   int *status)
{
	if (pid == 0)
		return SCE_KERNEL_ERROR_MODULEMGR_BAD_PID;
	if (flags != 0)
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	return StopUnloadModuleCommon(pid, modid, args, argp, LOAD_FLAG_FOR_PID, NULL, status);
}

/* name unknown */
SceUID loadStartSharedModuleCommon(SceUID pid, const char *path, SceSize args, void *argp,
					  SceUInt32 flags, const SceKernelLoadModuleOpt *opt,
					  int *status)
{
	SceUID modid;
	int ret;

	modid = loadModuleCommon(pid, path, flags | LOAD_FLAG_SHARED, NULL);
	if (modid < 0 || (flags & 0x100000) != 0)
		return modid;
	ret = startModuleCommon(pid, modid, args, argp, flags, NULL, status);
	if (ret == 0)
		return modid;
	if (ret == 1)
		return 0;
	if (ret != SCE_KERNEL_ERROR_MODULEMGR_START_FAILED && (SceUInt32)ret - 0x8002802c >= 2)
		StopUnloadModuleCommon(pid, modid, 0, NULL, 0x48000000, NULL, NULL);
	return ret;
}

SceUID SceModulemgrForDriver_E2ADEF8D(SceUID pid, const char *path, SceSize args,
					     void *argp, SceUInt32 flags,
					     const SceKernelLoadModuleOpt *opt, int *status)
{
	if (pid == 0)
		return SCE_KERNEL_ERROR_MODULEMGR_BAD_PID;
	return loadStartSharedModuleCommon(pid, path, args, argp, flags | LOAD_FLAG_FOR_PID, opt,
					   status);
}

int SceModulemgrForDriver_03B30B7E(SceUID modid, SceSize args, void *argp, SceUInt32 flags,
			       const SceKernelUnloadModuleOpt *opt, int *status)
{
	if ((flags & 0xbfffffff) != 0)
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	return StopUnloadModuleCommon(KERNEL_PID, modid, args, argp, flags, opt, status);
}

int sceKernelFinalizeKbl(void)
{
	int ret;

	if (g_private->kblModid <= 0)
		return 0;
	ret = StopUnloadModuleCommon(KERNEL_PID, g_private->kblModid, 0, NULL, 0, NULL, NULL);
	SCE_DBG_ASSERT_LEVEL(0, ret == 0, 1867);
	return ret;
}

int SceModulemgrForDriver_49A3EDC7(SceUID pid, SceUID modid, SceSize args, void *argp,
				     SceUInt32 flags, const SceKernelUnloadModuleOpt *opt, int *status)
{
	if (pid == 0)
		return SCE_KERNEL_ERROR_MODULEMGR_BAD_PID;
	if ((flags & 0xbfffffff) != 0)
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	return StopUnloadModuleCommon(pid, modid, args, argp, flags | LOAD_FLAG_FOR_PID, opt, status);
}

/* name unknown: the work queue entry of SceModulemgrForKernel_2F82EEBC */
int ModulemgrUnloadMySelfWork(void *arg)
{
	typeof(g_unloadMySelf) *req = arg;
	int ret;

	ret = sceKernelWaitThreadEnd(req->thid, NULL, NULL);
	if (ret < 0 && ret != 0x80028021)
		return ret;
	ret = StopUnloadModuleCommon(KERNEL_PID, req->modid, 0, NULL, 0x220, NULL, NULL);
	if (ret >= 0)
		sceGUIDReleaseObject(req->modid);
	return ret;
}

int SceModulemgrForDriver_100DAEB9(SceUID modid, SceSize args, void *argp, SceUInt32 flags,
			 const SceKernelStopModuleOpt *opt, int *status)
{
	if (flags != 0)
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	return stopModuleCommon(KERNEL_PID, modid, args, argp, 0, opt, status);
}

int sceKernelStopRemoteModule(SceUID pid, SceUID modid, SceSize args, void *argp,
			       SceUInt32 flags, const SceKernelStopModuleOpt *opt, int *status)
{
	if (pid == 0)
		return SCE_KERNEL_ERROR_MODULEMGR_BAD_PID;
	if (flags != 0)
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	return stopModuleCommon(pid, modid, args, argp, flags, opt, status);
}

int sceKernelRegisterLibrary(const void *libent)
{
	SceModuleObject *obj;
	SceUID modid;
	int ret;

	modid = LibraryDBGetModuleIdByAddr(KERNEL_PID, (SceUInt32)libent);
	if (modid < 0)
		return modid;
	obj = ModulemgrReferModule(modid);
	if (obj == NULL)
		return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
	ret = LibEntGetIndex(&obj->cb, (SceUInt32)libent);
	if (ret >= 0)
		ret = LoadcoreRegisterLibrary(&obj->cb, ret, 1);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 2995, "failed (res:0x%08x)\n", ret);
	sceGUIDReleaseObject(modid);
	SceCpuForKernel_803C84BF();
	return ret;
}

int sceKernelUnregisterLibrary(const void *libent)
{
	SceModuleObject *obj;
	SceUID modid;
	int ret;

	modid = LibraryDBGetModuleIdByAddr(KERNEL_PID, (SceUInt32)libent);
	if (modid < 0)
		return modid;
	obj = ModulemgrReferModule(modid);
	if (obj == NULL)
		return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
	ret = LibEntGetIndex(&obj->cb, (SceUInt32)libent);
	if (ret >= 0)
		ret = LoadcoreReleaseLibrary(&obj->cb, ret, 0);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3032, "LoadcoreReleaseLibrary : failed (res:0x%08x)\n",
				     ret);
	sceGUIDReleaseObject(modid);
	SceCpuForKernel_803C84BF();
	return ret;
}

SceModuleEntry SceModulemgrForKernel_7B302C5D(SceUID modid)
{
	SceModuleObject *obj = ModulemgrReferModule(modid);
	SceModuleEntry entry = NULL;

	if (obj != NULL) {
		entry = obj->cb.moduleStart;
		sceGUIDReleaseObject(modid);
	}
	return entry;
}

int SceModulemgrForKernel_37512E29(SceUID modid, SceModuleCB **cb)
{
	SceModuleObject *obj = ModulemgrReferModule(modid);

	if (obj == NULL)
		return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
	if (cb != NULL)
		*cb = &obj->cb;
	sceGUIDReleaseObject(modid);
	return 0;
}

int sceKernelGetMetaDataForDebugger(SceUID pid, SceUID modid, void **metadata, SceSize *size)
{
	SceModuleObject *obj;
	SceUID guid;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 3235);
	if (pid == KERNEL_PID || ModulemgrReferProcessObject(pid) < 0)
		return SCE_KERNEL_ERROR_MODULEMGR_NO_PROCESS;
	guid = SceSysmemForDriver_45D22597(pid, modid);
	if (guid < 0) {
		sceGUIDReleaseObject(pid);
		return guid;
	}
	obj = ModulemgrReferModule(guid);
	if (obj == NULL) {
		sceGUIDReleaseObject(pid);
		return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
	}
	*metadata = obj->cb.metadata;
	*size = obj->cb.metadataSize;
	sceGUIDReleaseObject(guid);
	sceGUIDReleaseObject(pid);
	return *metadata != NULL ? 0 : SCE_KERNEL_ERROR_MODULEMGR_NOENT;
}

static const PreloadModule g_preloadModules[] = {
	{ NULL, { "host0:module/libkernel.suprx", NULL }, "os0:us/libkernel.suprx", NULL,
	  "sd0:us/libkernel.suprx", "os0:us/libkernel.suprx", 0, 0x32 },
	{ NULL, { "host0:module/driver_us.suprx", NULL }, "os0:us/driver_us.suprx", NULL,
	  "sd0:us/driver_us.suprx", "os0:us/driver_us.suprx", 0, 0x32 },
	{ NULL, { "host0:module/avcodec_us.suprx", NULL }, "os0:us/avcodec_us.suprx", NULL,
	  "sd0:us/avcodec_us.suprx", "os0:us/avcodec_us.suprx", 0, 0x32 },
	{ NULL, { "host0:module/libgpu_es4.suprx", NULL }, "os0:us/libgpu_es4.suprx", NULL,
	  "sd0:us/libgpu_es4.suprx", "os0:us/libgpu_es4.suprx", 0, 0x32 },
	{ NULL, { "host0:module/libgxm_es4.suprx", NULL }, "os0:us/libgxm_es4.suprx", NULL,
	  "sd0:us/libgxm_es4.suprx", "os0:us/libgxm_es4.suprx", 0, 0x22 },
	{ NULL, { "host0:module/libgxm_dbg_es4.suprx", NULL },
	  "vs0:sys/external/libgxm_dbg_es4.suprx", NULL,
	  "sd0:us/libgxm_dbg_es4.suprx", "os0:us/libgxm_dbg_es4.suprx", 0, 0x18 },
	{ "SceFios2", { "host0:module/libfios2.suprx", NULL },
	  "vs0:sys/external/libfios2.suprx", "app0:sce_module/libfios2.suprx",
	  "sd0:us/libfios2.suprx", "os0:us/libfios2.suprx", 0x200000, 0x20000030 },
	{ "SceLibc", { "host0:module/libc.suprx", NULL },
	  "vs0:sys/external/libc.suprx", "app0:sce_module/libc.suprx",
	  "sd0:us/libc.suprx", "os0:us/libc.suprx", 0x10000, 0x10000030 },
	{ NULL, { "host0:module/libshellsvc.suprx", "host0:vs0/sys/external/libshellsvc.suprx" },
	  "vs0:sys/external/libshellsvc.suprx", NULL,
	  "sd0:us/libshellsvc.suprx", "os0:us/libshellsvc.suprx", 0x80000, 0x32 },
	{ NULL, { "host0:module/libcdlg.suprx", "host0:vs0/sys/external/libcdlg.suprx" },
	  "vs0:sys/external/libcdlg.suprx", NULL,
	  "sd0:us/libcdlg.suprx", "os0:us/libcdlg.suprx", 0x100000, 0x32 },
	{ NULL, { "host0:module/libdbg.suprx", NULL }, "vs0:sys/external/libdbg.suprx", NULL,
	  "sd0:us/libdbg.suprx", "os0:us/libdbg.suprx", 0x20000, 0x32 },
	{ NULL, { "host0:module/libSceFt2.suprx", "host0:vs0/sys/external/libSceFt2.suprx" },
	  "vs0:sys/external/libSceFt2.suprx", NULL,
	  "sd0:us/libSceFt2.suprx", "os0:us/libSceFt2.suprx", 0x800000, 0x32 },
	{ NULL, { "host0:module/libpvf.suprx", "host0:vs0/sys/external/libpvf.suprx" },
	  "vs0:sys/external/libpvf.suprx", NULL,
	  "sd0:us/libpvf.suprx", "os0:us/libpvf.suprx", 0x1000000, 0x32 },
	{ NULL, { "host0:module/apputil.suprx", "host0:vs0/sys/external/apputil.suprx" },
	  "vs0:sys/external/apputil.suprx", NULL,
	  "sd0:us/apputil.suprx", "os0:us/apputil.suprx", 0x400000, 0x30 },
	{ NULL, { "host0:module/libperf.suprx", "host0:vs0/sys/external/libperf.suprx" },
	  "vs0:sys/external/libperf.suprx", NULL,
	  "sd0:us/libperf.suprx", "os0:us/libperf.suprx", 0x2000000, 0xb8 },
};

int sceKernelLoadPreloadingModules(SceUID pid, const ProcessImageInfo *info,
				   SceUInt32 flags)
{
	const PreloadModule *m;
	char buf[0x40];
	const char *path;
	SceUInt32 lflags = LOAD_FLAG_8000;
	int tool, internal, i;
	SceUID ret;

	tool = sceKernelCheckDipsw(0xd2);
	if ((flags & 1) != 0)
		LibraryDBSetShared(pid);

	for (m = g_preloadModules; m != g_preloadModules + ARRAY_SIZE(g_preloadModules); m++) {
		if ((m->inhibitMask & info->processPreloadDisabled) != 0 ||
		    (m->depend != NULL && !LoadcoreIsLibraryImported(pid, m->depend) &&
		     (strncmp(m->depend, "SceFios2", 0x1f) != 0 ||
		      !LoadcoreIsLibraryImported(pid, "SceLibc")))) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 3300, "Skipped loading %s (no dependency)\n",
					     m->path);
			continue;
		}
		if ((m->flags & PRELOAD_DEVELOPMENT) != 0 && !SceSblACMgrForKernel_BBA13D9C())
			continue;
		if ((m->flags & PRELOAD_DIPSW_D4) != 0 && !sceKernelCheckDipsw(0xd4))
			continue;
		if ((m->flags & ((flags & 1) != 0 ? PRELOAD_STAGE1 : PRELOAD_STAGE0)) == 0)
			continue;
		if (tool)
			lflags |= (m->flags & PRELOAD_TOOL_SHARED) != 0 ? LOAD_FLAG_TYPE2 : 0;

		internal = 1;
		for (i = 0; i < 2; i++) {
			if (!SceSysrootForKernel_55392965()) {
				if (!SceQafMgrForDriver_B9770A13() || g_hostIoFailed ||
				    !sceKernelCheckDipsw(0xfb))
					continue;
				path = m->hostPath[i];
			} else {
				if (i == 0) {
					path = m->sdPath;
				} else {
					path = buf;
					snprintf(buf, sizeof(buf), "ux0:%s", m->sdPath + 4);
				}
				if (SceSysrootForKernel_89D19090())
					internal = 0;
			}
			if (path == NULL)
				continue;
			if ((m->flags & PRELOAD_SHARED) == 0)
				ret = SceModulemgrForKernel_4E85022D(pid, path, lflags, NULL);
			else
				ret = ModulemgrLoadSharedModuleForPid(pid, path, lflags);
			if (ret >= 0)
				goto next;
			if (ret == 0x80010002 || (ret & 0xfffeffff) == 0x8008000a) {
				if (!SceSysrootForKernel_55392965())
					continue;
				SCE_DBG_PRINTF_LEVEL(1, 0, 3391,
					"Preloading %s : Failed (0x%08x) (ignored)\n", path, ret);
			} else {
				SCE_DBG_PRINTF_LEVEL(1, 0, 3389,
					"Preloading %s : Failed (0x%08x) (ignored)\n", path, ret);
			}
		}

		if (!internal)
			goto notfound;
		path = m->path;
		if ((m->flags & PRELOAD_SHARED) != 0) {
			ret = ModulemgrLoadSharedModuleForPid(pid, path, lflags);
		} else {
			if (m->appPath != NULL) {
				if (!SceSblACMgrForKernel_sceSblACMgrIsGameProgram(pid) &&
				    !SceSblACMgrForKernel_3F99279F(pid)) {
					SceSysrootForKernel_EC3124A3(pid, buf, 0x20);
					if (strncmp(buf, "NPXS10007", 0x20) != 0)
						goto system;
				}
				if (SceModulemgrForKernel_4E85022D(pid, m->appPath, lflags | LOAD_FLAG_1000,
							       NULL) >= 0)
					goto next;
			}
system:
			if ((!SceSblACMgrForKernel_sceSblACMgrIsGameProgram(pid) && !SceSblACMgrForKernel_3F99279F(pid)) ||
			    m->appPath == NULL)
				ret = SceModulemgrForKernel_4E85022D(pid, path, lflags, NULL);
			else if ((flags & 2) == 0)
				ret = SceModulemgrForKernel_4E85022D(pid, path, lflags, NULL);
			else
				goto notfound;
		}
		if (ret >= 0)
			goto next;
notfound:
		if (m->appPath != NULL) {
			ret = (m->flags >> PRELOAD_ERROR_SHIFT) | 0x8002d0f0;
			SCE_PRINTF_WITH_INFO(0, 3442, "not found %s (0x%08x)\n", m->appPath, ret);
			return ret;
		}
next:
		SCE_DBG_PRINTF_LEVEL(1, 0, 3446, "");
	}
	relocatorFormLogPrint();
	return 0;
}

int sceKernelStartPreloadingModules(SceUID pid)
{
	SceModuleObject *obj;
	SceUID list[15];
	SceSize count;
	int ret;

	SCE_DBG_ASSERT_LEVEL(0, pid != 0, 3462);
	count = ARRAY_SIZE(list);
	ret = sceKernelGetModuleList(pid, 0x80, 1, list, &count);
	if (ret < 0 || count == 0)
		return ret;
	do {
		count--;
		obj = ModulemgrReferModule(list[count]);
		if (obj == NULL)
			continue;
		SCE_DBG_PRINTF_LEVEL(1, 0, 3484, "Preloading %s (guid=0x%08x, fingerprint=0x%08x)\n",
				     obj->cb.path, list[count], obj->cb.fingerprint);
		ret = startModuleCommon(pid, list[count], 0, NULL,
					SceSysrootForKernel_55392965() ? 0x4000000 : 0, NULL, NULL);
		sceGUIDReleaseObject(list[count]);
		if (ret < 0 && !SceSysrootForKernel_55392965())
			return ret;
	} while (count != 0);
	return 0;
}

static const SysrootModulemgrOps g_sysrootCallbacks = {
	.size = sizeof(SysrootModulemgrOps),
	.getStubInfo = ModulemgrGetStubInfo,
	.showCallerInfo = ShowCallerInfo,
	.getModuleInfoMinByAddr = SceModulemgrForKernel_5564A860,
	.backtrace = SceBacktraceForDriver_888E99B8,
	.printBacktrace = SceBacktraceForDriver_7C878F90,
};

int SceModulemgrForKernel_2F82EEBC(void)
{
	int ret;

	relocatorFormLogPrint();
	g_unloadMySelf.thid = sceKernelGetThreadId();
	g_unloadMySelf.modid = LibraryDBGetModuleIdByAddr(KERNEL_PID,
							  (SceUInt32)__builtin_return_address(0));
	ret = sceKernelScheduleWorkTask(0x10023, "SceKernelUnloadMySelf",
					 ModulemgrUnloadMySelfWork, &g_unloadMySelf);
	if (ret < 0)
		return ret;
	ret = sceKernelExitDeleteThread(0);
	return ret < 0 ? ret : 0;
}

/* name unknown: sceKernelInhibitLoadingModule */
int ModulemgrInhibitLoadingModule(SceUInt32 flags)
{
	LibraryDB *db;
	SceUID pid;
	int intr;

	if ((flags & ~LIBDB_FLAG_INHIBIT_MASK) != 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	pid = sceKernelGetProcessId();
	if (pid == KERNEL_PID)
		return SCE_KERNEL_ERROR_MODULEMGR_BAD_PID;
	db = LibraryDBLock(pid, &intr);
	if (db == NULL)
		return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
	if ((int)(db->flags & LIBDB_FLAG_INHIBIT_MASK) < (int)flags) {
		db->flags = (db->flags & ~LIBDB_FLAG_INHIBIT_MASK) | flags;
		LibraryDBUnlock(db, intr);
		return 0;
	}
	LibraryDBUnlock(db, intr);
	return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
}

int ModulemgrModuleStart(SceSize args, void *argp)
{
	SceKernelProcEventHandler handler;
	int ret;

	g_mutex = sceKernelCreateMutex("SceModuleMgr", 0x2000, 0, NULL);
	if (g_mutex < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3654, "create mutex failed (0x%08x)\n", g_mutex);
		SCE_DBG_ASSERT_LEVEL(0, 0, 3655);
	}
	ret = SelfDecrypterInit();
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 3660);

	g_moduleClass = sceKernelSysrootGetModulePrivate(9);
	g_moduleClass->name = "SceUIDModuleClass";
	g_moduleClass->create_cb = ModulemgrConstructor;
	g_moduleClass->destroy_cb = ModulemgrDestructor;
	g_private = (ModulemgrPrivate *)g_moduleClass;
	g_private->stubClass.name = "SceUIDLibStubClass";
	g_libraryClass = &g_private->libraryClass;
	g_private->stubClass.create_cb = LibStubClassCreate;
	g_private->stubClass.destroy_cb = LibStubClassDestroy;
	g_stubClass = &g_private->stubClass;
	g_private->libraryClass.name = "SceUIDLibraryClass";
	g_private->libraryClass.create_cb = LibraryClassCreate;
	g_private->libraryClass.destroy_cb = LibraryClassDestroy;
	sceKernelSetProcessLibdb(KERNEL_PID, &g_private->kernelDB);
	SceSysrootForKernel_B171CC2D(&g_sysrootCallbacks);

	g_threadStateSlot = SceThreadmgrForDriver_B645C7EF(sizeof(ModulemgrThreadState), 0, 0);
	SCE_DBG_ASSERT(g_threadStateSlot >= 0, 3679);

	handler.size = sizeof(handler);
	handler.create = NULL;
	handler.exit = NULL;
	handler.kill = NULL;
	handler.suspend = NULL;
	handler.resume = NULL;
	handler.notify = ModulemgrProcEventHandler;
	ret = sceKernelRegisterProcEventHandler("SceModulemgr", &handler, NULL);
	if (ret < 0)
		SCE_DBG_ASSERT_LEVEL(0, 0, 3689);
	BacktraceInit();
	return 0;
}

int SceModulemgrForKernel_E71530D7(SceUID pid)
{
	LibraryDB *db;
	SceUID modid;
	int intr;

	for (;;) {
		db = LibraryDBLock(pid, &intr);
		if (db == NULL)
			return SCE_KERNEL_ERROR_LIBRARYDB_NOENT;
		if (db->modules == NULL)
			break;
		modid = db->modules->modid;
		LibraryDBUnlock(db, intr);
		StopUnloadModuleCommon(pid, modid, 0, NULL, 0x40000330, NULL, NULL);
	}
	LibraryDBUnlock(db, intr);
	LibraryDBDelete(pid);
	return 0;
}

static inline int ModulemgrImportProcParam(SceModuleCB *cb, const SceProcessParam *uparam,
					   ProcessImageInfo *info)
{
	SceProcessParam param;
	SceSize size;
	int ret;

	memset(&param, 0, sizeof(param));
	ret = sceKernelCopyFromUserProc(cb->pid, &size, uparam, sizeof(size));
	if (ret < 0)
		return ret;
	if (size > sizeof(param)) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 810,
			"WARNING: module %s uses unknown procparam (size=%d)\n", cb->path, size);
		size = sizeof(param);
	} else if ((size & ~4u) == 0x30 || size == 0x28) {
		/* current */
	} else if (size == 0x2c || (size & ~4u) == 0x20) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 824,
			"WARNING: module %s uses obsolete procparam (size=%d)\n", cb->path, size);
	} else {
		return SCE_KERNEL_ERROR_MODULEMGR_INVALID_PROC_PARAM;
	}

	ret = ModulemgrCopyFromUser(cb, &param, uparam, size);
	if (ret < 0)
		return ret;
	if (param.magic != SCE_PROCESS_PARAM_MAGIC)
		return SCE_KERNEL_ERROR_MODULEMGR_INVALID_PROC_PARAM;
	if (param.mainThreadPriority != NULL &&
	    (ret = ModulemgrCopyFromUser(cb, &info->mainThreadPriority,
					 param.mainThreadPriority, 4)) < 0)
		return ret;
	if (param.mainThreadStackSize != NULL &&
	    (ret = ModulemgrCopyFromUser(cb, &info->mainThreadStackSize,
					 param.mainThreadStackSize, 4)) < 0)
		return ret;
	if (param.mainThreadAttr != NULL &&
	    (ret = ModulemgrCopyFromUser(cb, &info->mainThreadAttr, param.mainThreadAttr, 4)) < 0)
		return ret;
	if (param.mainThreadCpuAffinityMask != NULL &&
	    (ret = ModulemgrCopyFromUser(cb, &info->mainThreadCpuAffinityMask,
					 param.mainThreadCpuAffinityMask, 4)) < 0)
		return ret;
	if (param.mainThreadName != NULL &&
	    (ret = ModulemgrStrncpyFromUser(cb, info->mainThreadName, param.mainThreadName)) < 0)
		return ret;
	if (param.processName != NULL &&
	    (ret = ModulemgrStrncpyFromUser(cb, info->processName, param.processName)) < 0)
		return ret;
	if (param.processPreloadDisabled != NULL &&
	    (ret = ModulemgrCopyFromUser(cb, &info->processPreloadDisabled,
					 param.processPreloadDisabled, 4)) < 0)
		return ret;
	if (param.mainThreadOptParam != NULL) {
		ret = ModulemgrCopyFromUser(cb, &size, param.mainThreadOptParam, 4);
		if (ret < 0)
			return ret;
		if (size > 0x1c)
			return SCE_KERNEL_ERROR_MODULEMGR_INVALID_PROC_PARAM;
		ret = ModulemgrCopyFromUser(cb, info->mainThreadOptParam, param.mainThreadOptParam, size);
		if (ret < 0)
			return ret;
	}
	info->processParam = (void *)uparam;
	info->sdkVersion = param.sdkVersion;
	cb->sdkVersion = param.sdkVersion;
	return 0;
}

SceUID sceKernelLoadProcessImage(SceUID pid, const char *path, SceUInt32 flags,
				 SceSelfAuthInfo *auth, ProcessImageInfo *info, const ModulemgrTitlePatch *patch)
{
	SceModuleObject *obj;
	LibraryDB *db;
	SceUInt32 t0, t1;
	SceUID fd, old;
	int perm, ret;

	if (pid == KERNEL_PID)
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	perm = sceKernelSetThreadAccessLevel(0x80);
	old = SceThreadmgrForDriver_0486F239(KERNEL_PID);
	SceKernelSuspendForDriver_4DF40893(0);
	fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0) {
		SceKernelSuspendForDriver_2BB92967(0);
		SceThreadmgrForDriver_0486F239(old);
		sceKernelSetThreadAccessLevel(perm);
		return fd;
	}
	db = LibraryDBInitializeAddr2ModuleTable(pid);
	if (db == NULL) {
		sceIoClose(fd);
		SceKernelSuspendForDriver_2BB92967(0);
		SceThreadmgrForDriver_0486F239(old);
		sceKernelSetThreadAccessLevel(perm);
		return SCE_KERNEL_ERROR_MODULEMGR_NOMEM_LIB;
	}
	sceKernelSetProcessLibdb(pid, db);
	ret = ModuleObjNew(pid, flags | LOAD_FLAG_PROCESS_IMAGE, &obj);
	if (ret < 0) {
		LibraryDBDelete(pid);
		sceIoClose(fd);
		SceKernelSuspendForDriver_2BB92967(0);
		SceThreadmgrForDriver_0486F239(old);
		sceKernelSetThreadAccessLevel(perm);
		return ret;
	}
	t0 = SceThreadmgrForDriver_47F6DE49();
	ret = ModulemgrLoadModule(obj, path, fd, auth, flags | LOAD_FLAG_PROCESS_IMAGE);
	t1 = SceThreadmgrForDriver_47F6DE49();
	sceIoClose(fd);
	SceKernelSuspendForDriver_2BB92967(0);
	SceThreadmgrForDriver_0486F239(old);
	sceKernelSetThreadAccessLevel(perm);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3108, "load (%s) failed (0x%08x)\n", path, ret);
		LibraryDBDelete(pid);
		return ret;
	}
	SCE_DBG_PRINTF_LEVEL(1, 0, 3118,
		"Loading %s (guid=0x%08x, fingerprint=0x%08x): ProcessImage OK.   %d usec (form 0x%03x)\n",
		path, obj->cb.modid, obj->cb.fingerprint, t1 - t0, g_relocForms);
	(void)t0;
	(void)t1;
	relocatorFormCount();
	ModuleCBDumpLoadAddress(&obj->cb);

	if (sceKernelCheckDipsw(0xd2))
		LibraryDBRenameAddr2ModTbls(obj->cb.pid, obj->cb.name);
	if (obj->cb.procParam != NULL) {
		ret = ModulemgrImportProcParam(&obj->cb, obj->cb.procParam, info);
		if (ret < 0)
			goto unload;
	} else if ((obj->cb.flags & MODULE_FLAG_SELF_FIXED) != 0) {
		SceModulemgrForKernel_E71530D7(pid);
		return SCE_KERNEL_ERROR_MODULEMGR_NO_PROCPARAM;
	}

	ret = ModulemgrStartModule(&obj->cb, 0, NULL, 0, NULL, patch);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3157, "start failed (res:0x%08x)\n", ret);
		goto unload;
	}
	if (SceSblACMgrForKernel_sceSblACMgrIsGameProgram(pid) &&
	    (ret = SceSblAuthMgrForKernel_ABAB8466(obj->cb.sdkVersion)) < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3171, "unknown SDK version (%08x)\n",
				     obj->cb.sdkVersion);
		goto unload;
	}
	db->mainModid = obj->cb.modid;
	relocatorFormLogPrint();
	return obj->cb.modid;

unload:
	SceModulemgrForKernel_E71530D7(pid);
	return ret;
}

int sceKernelUnlinkNormalSyscall(SceUID pid)
{
	int count, i, ret;

	if (pid == KERNEL_PID || pid == 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = ModulemgrReferProcessObject(pid);
	if (ret < 0)
		return ret;
	ModulemgrLockLibDB();
	count = ARRAY_SIZE(g_unlinkList);
	ret = LibraryDBGetModuleCBList(pid, g_unlinkList, &count);
	if (ret < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 3762);
	} else {
		for (i = 0; i < count; i++) {
			ret = ModulemgrReleaseClients(g_unlinkList[i], 0x300000);
			if (ret < 0)
				return ret;
		}
		SceCpuForKernel_803C84BF();
	}
	ModulemgrUnlockLibDB();
	sceGUIDReleaseObject(pid);
	return 0;
}

SceUID SceModulemgrForDriver_BBE1771C(const char *name)
{
	return LibraryDBFindModuleByName(KERNEL_PID, name);
}

int sceKernelGetSystemSwVersion(SceKernelSystemSwVersion *info)
{
	SceUInt32 v;

	if (info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!g_fwInfoValid) {
		v = SceSysrootForDriver_67AAB627();
		g_fwInfo.version = v;
		g_fwInfo.versionExtra = 0;
		g_fwInfo.size = sizeof(g_fwInfo);
		snprintf(g_fwInfo.versionString, sizeof(g_fwInfo.versionString), "%d.%02d",
			 ((v >> 28) & 0xf) * 10 + ((v >> 24) & 0xf),
			 ((v >> 20) & 0xf) * 10 + ((v >> 16) & 0xf));
		g_fwInfoValid++;
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 3824, "[%s]\n", g_fwInfo.versionString);
	}
	*info = g_fwInfo;
	return 0;
}

/* name unknown */
int ModulemgrCopyIn(SceUID pid, void *dst, const void *src, SceSize len)
{
	if (pid != KERNEL_PID)
		return sceKernelCopyFromUserProc(pid, dst, src, len);
	memcpy(dst, src, len);
	return 0;
}

/* name unknown */
void *ModulemgrAlloc(SceUID pid, SceSize size)
{
	if (pid != KERNEL_PID)
		return sceKernelAllocRemoteProcessHeap(pid, size, NULL);
	return ModulemgrKernelAlloc(size);
}

/* name unknown */
void ModulemgrFree(SceUID pid, void *ptr)
{
	if (pid != KERNEL_PID)
		sceKernelFreeRemoteProcessHeap(pid, ptr);
	else
		ModulemgrKernelFree(ptr);
}
