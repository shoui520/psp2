/*
 * SceDeci4pSdbgpTest and SceDeci4pSdbgpForKernel test helpers: build timestamp
 * and SVN revision queries, NameHeap log save, physical memory free size, and
 * user-mode module/process/budget test syscalls.
 */

#include <string.h>
#include <fw.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/modulemgr.h>
#include <kernel/threadmgr.h>
#include <modulemgr/modulemgr.h>
#include <modulemgr/modulemgr_debugger.h>
#include <processmgr/processmgr.h>
#include <sysmem/cp15.h>
#include <sysmem/debug.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include "sdbgp_internal.h"

#if FW_VERSION >= 0x04000000
static const char s_buildDate[] = "2017-02-23 19:46:03+0900";
static volatile const uint32_t s_svnRevision = 96726;
#elif FW_VERSION >= 0x03650000
static const char s_buildDate[] = "2017-03-17 06:20:48+0900";
static volatile const uint32_t s_svnRevision = 96745;
#else
static const char s_buildDate[] = "2016-10-24 06:26:38+0900";
static volatile const uint32_t s_svnRevision = 96393;
#endif

typedef struct TestLoadModuleArg {
	const char *path;
	SceUInt32 flags;
	const SceKernelLoadModuleOpt *opt;
} TestLoadModuleArg;

typedef struct TestStartModuleArg {
	SceUID modid;
	SceSize args;
	void *argp;
	SceUInt32 flags;
	const SceKernelStartModuleOpt *opt;
	int *status;
} TestStartModuleArg;

typedef struct SceTestModuleOpt {
	SceSize size;
	uint32_t data[64];
	uint32_t flags;
	uint32_t reserved;
} SceTestModuleOpt;

_Static_assert(sizeof(SceTestModuleOpt) == 0x10c, "SceTestModuleOpt size");

static int LoadModuleThread(SceSize args, void *argp)
{
	TestLoadModuleArg *arg = argp;
	SceUID modid;

	modid = sceKernelLoadModule(arg->path, arg->flags, arg->opt);
	if (modid < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 195,
			"*** error: sceKernelLoadModule(\"%s\", 0x%08X, %p)=0x%08X\n",
			arg->path, arg->flags, arg->opt, modid);
	}
	return modid;
}

static int StartModuleThread(SceSize args, void *argp)
{
	TestStartModuleArg *arg = argp;
	int res;

	res = sceKernelStartModule(arg->modid, arg->args, arg->argp,
				   arg->flags, arg->opt, arg->status);
	if (res < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 312,
			"*** error: sceKernelStartModule(0x%08X, 0x%08X, %p, 0x%08X, %p, %p)=0x%08X\n",
			arg->modid, arg->args, arg->argp, arg->flags, arg->opt,
			arg->status, res);
	}
	return res;
}

uint32_t SceDeci4pSdbgpForKernel_7B25F7A5(void)
{
	return s_svnRevision;
}

int SceDeci4pSdbgpForKernel_762BFAE0(SceSize max, char *dst)
{
	strncpy(dst, s_buildDate, strnlen(s_buildDate, max));
	return 0;
}

int SceDeci4pSdbgpForKernel_17E9BFED(void)
{
	return NameHeapLogSaveCore();
}

int SceDeci4pSdbgpTest_6A5AC725(void)
{
	return 0;
}

uint32_t SceDeci4pSdbgpTest_B671059C(void)
{
	return s_svnRevision;
}

int SceDeci4pSdbgpTest_50DA251E(SceSize max, void *udst)
{
	return sceKernelCopyToUser(udst, s_buildDate, strnlen(s_buildDate, max));
}

void SceDeci4pSdbgpTest_B55D9F76(void)
{
	volatile int val = *(int *)0;
	(void)val;
}

int SceDeci4pSdbgpTest_462DC57D(SceUInt32 type, const void *addr, SceSize size)
{
	if (!SceQafMgrForDriver_382C71E8())
		return SCE_KERNEL_ERROR_NOSYS;
	return sceKernelIsAccessibleRange(type, addr, size);
}

SceSize SceDeci4pSdbgpTest_462B520A(void)
{
	SceKernelPhyMemPartInfoForDebugger info;
	int res;

	info.size = sizeof(info);
	info.freeBlockCount = 0;
	info.freeBlockMax = 0;
	info.freeBlocks = NULL;
	res = sceKernelGetPhyMemPartInfoForDebugger(SceSysmemForKernel_62800A6B(), &info);
	return res < 0 ? 0 : info.freeSize;
}

SceUID sceTestLoadModuleForUser(const char *upath)
{
	uint32_t tpidruro;
	int res;
	SceUID thid;
	int exitStatus;
	TestLoadModuleArg arg;
	char path[1024];

	tpidruro = readTpidruro();
	writeTpidruro(tpidruro << 16);
	if (!SceQafMgrForDriver_382C71E8()) {
		res = SCE_KERNEL_ERROR_NOSYS;
		goto out;
	}
	if (!upath) {
		res = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto out;
	}
	res = sceKernelStrncpyFromUser(path, upath, sizeof(path));
	if (res < 0)
		goto out;
	thid = sceKernelCreateThread("TestLoadModuleThread", LoadModuleThread,
				     0x10000100, 0x1000, 0, 0xf, NULL);
	if (thid < 0) {
		res = thid;
		SCE_DBG_PRINTF_LEVEL(1, 0, 243,
			"*** error: sceKernelCreateThread(\"TestLoadModuleThread\")=0x%08X\n",
			thid);
		goto out;
	}
	arg.path = path;
	arg.flags = 0;
	arg.opt = NULL;
	res = sceKernelStartThread(thid, sizeof(arg), &arg);
	if (res < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 254,
			"*** error: sceKernelStartThread(0x%08X)=0x%08X\n",
			thid, res);
	} else {
		exitStatus = -1;
		res = sceKernelWaitThreadEnd(thid, &exitStatus, NULL);
		if (res != (int)0x80028021 && res < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 263,
				"*** error: sceKernelWaitThreadEnd(0x%08X)=0x%08X\n",
				thid, res);
		} else {
			if (res < 0) {
				res = exitStatus;
				goto open_puid;
			}
			res = exitStatus;
			if (res >= 0) {
open_puid:
				res = scePUIDOpenByGUID(0, res);
				if (res < 0) {
					SCE_DBG_PRINTF_LEVEL(1, 0, 275,
						"*** error: scePUIDOpenByGUID(0x%08X, 0x%08X)=0x%08X\n",
						0, exitStatus, res);
					sceGUIDClose(exitStatus);
				}
			}
		}
	}
	sceKernelDeleteThread(thid);
out:
	writeTpidruro(tpidruro);
	return res;
}

int sceTestStartModuleForUser(SceUID puid, SceSize args, const void *uargp, int *ustatus)
{
	uint32_t tpidruro;
	int res;
	SceTestModuleOpt *opt;
	SceUID thid;
	int status;
	int exitStatus;
	TestStartModuleArg startArg;

	tpidruro = readTpidruro();
	writeTpidruro(tpidruro << 16);
	res = SCE_KERNEL_ERROR_NOSYS;
	if (!SceQafMgrForDriver_382C71E8())
		goto out;
	if (!uargp) {
		res = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	if (args < sizeof(SceTestModuleOpt)) {
		res = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto out;
	}
	res = scePUIDtoGUID(0, puid);
	if (res < 0)
		goto out;
	opt = sceKernelAllocOwnProcessHeap(args, NULL);
	if (!opt) {
		res = SCE_KERNEL_ERROR_NO_MEMORY;
		SCE_DBG_PRINTF_LEVEL(1, 0, 362,
			"*** error: sceKernelAllocOwnProcessHeap(0x%08X, %p)=%p\n",
			args, NULL, NULL);
		goto out;
	}
	{
		int copyRes = sceKernelCopyFromUser(opt, uargp, args);

		if (copyRes < 0) {
			res = copyRes;
			goto free_opt;
		}
	}
	if (opt->size != args) {
		res = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto free_opt;
	}
	if (opt->flags & 2) {
		thid = sceKernelCreateThread("TestStartModuleThread",
			StartModuleThread, 0x10000100, 0x10000, 0, 0xf, NULL);
		if (thid < 0) {
			res = thid;
			SCE_DBG_PRINTF_LEVEL(1, 0, 386,
				"*** error: sceKernelCreateThread(\"TestStartModuleThread\")=0x%08X\n",
				thid);
			goto free_opt;
		}
		startArg.modid = res;
		startArg.args = opt->size;
		startArg.argp = opt;
		startArg.flags = 0;
		startArg.opt = NULL;
		startArg.status = ustatus ? &status : NULL;
		res = sceKernelStartThread(thid, sizeof(startArg), &startArg);
		if (res < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 404,
				"*** error: sceKernelStartThread(0x%08X)=0x%08X\n",
				thid, res);
			sceKernelDeleteThread(thid);
			goto free_opt;
		}
		res = sceKernelWaitThreadEnd(thid, &exitStatus, NULL);
		if (res < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 412,
				"*** error: sceKernelWaitThreadEnd(0x%08X)=0x%08X\n",
				thid, res);
			goto free_opt;
		}
		res = sceKernelDeleteThread(thid);
		if (res < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 419,
				"*** error: sceKernelDeleteThread(0x%08X)=0x%08X\n",
				thid, res);
			goto free_opt;
		}
		if (exitStatus < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 425,
				"*** error: StartModuleThread, exitStatus=0x%08X\n",
				exitStatus);
			res = exitStatus;
			goto free_opt;
		}
		if (ustatus) {
			int copyRes = sceKernelCopyToUser(ustatus, &status, sizeof(status));

			res &= ~(copyRes >> 31);
			if (copyRes < 0)
				res = copyRes;
		}
		goto free_opt;
	}
	if (!(opt->flags & 1)) {
		res = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
		goto free_opt;
	}
	{
		int *statusPtr = ustatus ? &status : NULL;

		res = sceKernelStartModule(res, opt->size, opt, 0, NULL, statusPtr);
		if (res < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 448,
				"*** error: sceKernelStartModule(0x%08X, 0x%08X, %p, 0x%08X, %p, %p)=0x%08X\n",
				puid, opt->size, opt, 0, NULL, statusPtr, res);
			goto free_opt;
		}
		if (ustatus) {
			int copyRes = sceKernelCopyToUser(ustatus, &status, sizeof(status));

			res &= ~(copyRes >> 31);
			if (copyRes < 0)
				res = copyRes;
		}
	}
free_opt:
	SceProcessmgrForDriver_FC2A424E(opt);
out:
	writeTpidruro(tpidruro);
	return res;
}

int sceTestStopModuleForUser(SceUID puid, SceSize args, const void *uargp, int *ustatus)
{
	uint32_t tpidruro;
	int res;
	SceTestModuleOpt *opt;
	int status;

	tpidruro = readTpidruro();
	writeTpidruro(tpidruro << 16);
	res = SCE_KERNEL_ERROR_NOSYS;
	if (!SceQafMgrForDriver_382C71E8())
		goto out;
	if (!uargp) {
		res = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	if (args < sizeof(SceTestModuleOpt)) {
		res = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto out;
	}
	res = scePUIDtoGUID(0, puid);
	if (res < 0)
		goto out;
	opt = sceKernelAllocOwnProcessHeap(args, NULL);
	if (!opt) {
		res = SCE_KERNEL_ERROR_NO_MEMORY;
		SCE_DBG_PRINTF_LEVEL(1, 0, 509,
			"*** error: sceKernelAllocOwnProcessHeap(0x%08X, %p)=%p\n",
			args, NULL, NULL);
		goto out;
	}
	{
		int copyRes = sceKernelCopyFromUser(opt, uargp, args);

		if (copyRes < 0) {
			res = copyRes;
			goto free_opt;
		}
	}
	if (opt->size != args) {
		res = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto free_opt;
	}
	{
		int *statusPtr = ustatus ? &status : NULL;

		res = sceKernelStopRemoteModule(SDBGP_KERNEL_PROCESS_ID, res,
						opt->size, opt, 0, NULL, statusPtr);
		if (res < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 532,
				"*** error: sceKernelStopRemoteModule(0x%08X, 0x%08X, 0x%08X, %p, 0x%08X, %p, %p)=0x%08X\n",
				sceKernelGetProcessId(), puid, opt->size, opt,
				0, NULL, statusPtr, res);
			goto free_opt;
		}
		if (ustatus) {
			int copyRes = sceKernelCopyToUser(ustatus, &status, sizeof(status));

			res &= ~(copyRes >> 31);
			if (copyRes < 0)
				res = copyRes;
		}
	}
free_opt:
	SceProcessmgrForDriver_FC2A424E(opt);
out:
	writeTpidruro(tpidruro);
	return res;
}

int sceTestUnloadModuleForUser(SceUID puid)
{
	uint32_t tpidruro;
	int res;
	int unloadRes;

	tpidruro = readTpidruro();
	writeTpidruro(tpidruro << 16);
	res = SCE_KERNEL_ERROR_NOSYS;
	if (SceQafMgrForDriver_382C71E8()) {
		res = scePUIDtoGUID(0, puid);
		if (res >= 0) {
			unloadRes = sceKernelUnloadModule(res, 0, NULL);
			if (unloadRes < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 574,
					"*** error: sceKernelUnloadModule(0x%08X, 0x%08X, %p)=0x%08X\n",
					res, 0, NULL, unloadRes);
				res = unloadRes;
			} else {
				res = scePUIDClose(0, puid);
			}
		}
	}
	writeTpidruro(tpidruro);
	return res;
}

SceUID sceTestCreateProcessForUser(const char *uname, SceUInt32 type,
				   const char *upath, const SceKernelProcessOpt *uopt)
{
	uint32_t tpidruro;
	int res;
	char *name;
	char *path;
	SceKernelProcessOpt opt;

	tpidruro = readTpidruro();
	writeTpidruro(tpidruro << 16);
	if (!SceQafMgrForDriver_382C71E8()) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 638, "Prologue failed 0x%08x\n",
				     SCE_KERNEL_ERROR_NOSYS);
		writeTpidruro(tpidruro);
		return SCE_KERNEL_ERROR_NOSYS;
	}
	name = (char *)uname;
	if (uname) {
		name = SceSysmemForKernel_85571907(256);
		res = sceKernelStrncpyFromUser(name, uname, 256);
		if (res == 256)
			res = 0x8001005b;
		if (res < 0) {
			writeTpidruro(tpidruro);
			return res;
		}
	}
	path = NULL;
	if (upath) {
		path = SceSysmemForKernel_85571907(256);
		res = sceKernelStrncpyFromUser(path, upath, 256);
		if (res == 256)
			res = 0x8001005b;
		if (res < 0) {
			if (name)
				SceSysmemForKernel_4233C16D(name);
			writeTpidruro(tpidruro);
			return res;
		}
	}
	if (uopt) {
		res = sceKernelCopyFromUser(&opt, uopt, sizeof(opt));
		if (res >= 0)
			res = sceKernelCreateProcess(name, type, path, &opt);
	} else {
		res = sceKernelCreateProcess(name, type, path, uopt);
	}
	if (name)
		SceSysmemForKernel_4233C16D(name);
	if (path)
		SceSysmemForKernel_4233C16D(path);
	writeTpidruro(tpidruro);
	return res;
}

int sceTestStartProcessForUser(SceUID pid, int waitDebugger, SceSize args, void *argp)
{
	uint32_t tpidruro;
	int res;

	tpidruro = readTpidruro();
	writeTpidruro(tpidruro << 16);
	if (!SceQafMgrForDriver_382C71E8()) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 709, "Prologue failed 0x%08x\n",
				     SCE_KERNEL_ERROR_NOSYS);
		writeTpidruro(tpidruro);
		return SCE_KERNEL_ERROR_NOSYS;
	}
	res = sceKernelStartProcess(pid, waitDebugger, args, argp);
	writeTpidruro(tpidruro);
	return res;
}

int sceTestWaitProcessEndForUser(SceUID pid, int *uexitStatus, SceUInt32 *utimeout)
{
	uint32_t tpidruro;
	int res;
	int exitStatus;
	SceUInt32 timeout;

	tpidruro = readTpidruro();
	writeTpidruro(tpidruro << 16);
	if (!SceQafMgrForDriver_382C71E8()) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 735, "Prologue failed 0x%08x\n",
				     SCE_KERNEL_ERROR_NOSYS);
		writeTpidruro(tpidruro);
		return SCE_KERNEL_ERROR_NOSYS;
	}
	if (!utimeout ||
	    (res = sceKernelCopyFromUser(&timeout, utimeout, sizeof(timeout))) >= 0) {
		res = sceKernelWaitProcessEnd(pid, &exitStatus, NULL);
		if ((!uexitStatus ||
		     (res = sceKernelCopyToUser(uexitStatus, &exitStatus, sizeof(exitStatus))) >= 0) &&
		    utimeout) {
			res = sceKernelCopyToUser(utimeout, &timeout, sizeof(timeout));
		}
	}
	writeTpidruro(tpidruro);
	return res;
}

int sceTestGetModuleNumForUser(void)
{
	uint32_t tpidruro;
	int res;

	tpidruro = readTpidruro();
	writeTpidruro(tpidruro << 16);
	if (!SceQafMgrForDriver_382C71E8()) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 779, "Prologue failed 0x%08x\n",
				     SCE_KERNEL_ERROR_NOSYS);
		writeTpidruro(tpidruro);
		return SCE_KERNEL_ERROR_NOSYS;
	}
	res = sceKernelGetModuleInfoForDebugger(sceKernelGetProcessId(), NULL, NULL);
	writeTpidruro(tpidruro);
	return res;
}

SceUID sceTestCreateBudgetForUser(const char *uname, SceUInt32 type,
				  const ProcessBudgetOpt *uopt)
{
	uint32_t tpidruro;
	int res;
	char *name;
	ProcessBudgetOpt opt;

	tpidruro = readTpidruro();
	writeTpidruro(tpidruro << 16);
	if (!SceQafMgrForDriver_382C71E8()) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 870, "Prologue failed 0x%08x\n",
				     SCE_KERNEL_ERROR_NOSYS);
		writeTpidruro(tpidruro);
		return SCE_KERNEL_ERROR_NOSYS;
	}
	if (!uname) {
		int copyRes = sceKernelCopyFromUser(&opt, uopt, sizeof(opt));

		if (copyRes < 0) {
			writeTpidruro(tpidruro);
			return copyRes;
		}
		res = sceKernelCreateBudget(NULL, type, &opt);
		writeTpidruro(tpidruro);
		return res;
	}
	name = SceSysmemForKernel_85571907(256);
	res = sceKernelStrncpyFromUser(name, uname, 256);
	if (res == 256)
		res = 0x8001005b;
	if (res >= 0) {
		int copyRes = sceKernelCopyFromUser(&opt, uopt, sizeof(opt));

		if (copyRes < 0) {
			if (name)
				SceSysmemForKernel_4233C16D(name);
			writeTpidruro(tpidruro);
			return copyRes;
		}
		res = sceKernelCreateBudget(name, type, &opt);
		if (name)
			SceSysmemForKernel_4233C16D(name);
	}
	writeTpidruro(tpidruro);
	return res;
}

int sceTestDeleteBudgetForUser(SceUID budgetId)
{
	uint32_t tpidruro;
	int res;

	tpidruro = readTpidruro();
	writeTpidruro(tpidruro << 16);
	if (!SceQafMgrForDriver_382C71E8()) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 912, "Prologue failed 0x%08x\n",
				     SCE_KERNEL_ERROR_NOSYS);
		writeTpidruro(tpidruro);
		return SCE_KERNEL_ERROR_NOSYS;
	}
	res = sceKernelDeleteBudget(budgetId);
	writeTpidruro(tpidruro);
	return res;
}
