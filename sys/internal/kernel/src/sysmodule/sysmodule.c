/*
 * SceSysmodule: sceSysmoduleLoadModule() and friends. A module ID names a list
 * of user libraries (tables.h); loading walks the list from its end, loading
 * each library not yet loaded for the process, and unloading walks it forward.
 * Per-process state (reference counts, progress) lives in process local
 * storage, so a load interrupted by a stopped or suspended thread resumes where
 * it was.
 */

#include <fw.h>
#include "sysmodule_ids.h"
#include <sysmem/dipsw.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/procevent.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <acmgr/acmgr.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <processmgr/processmgr.h>
#include <sysmem/qafmgr.h>
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr.h>
#include <stddef.h>

#define SYSMODULE_LIBS		0x7d
#define SYSMODULE_MODULES	0x56		/* public IDs */
#define SYSMODULE_INTERNAL	0x2a		/* IDs with bit 31 set */
#define SYSMODULE_PATH_MAX	0x40

#define SYSMODULE_ID_INTERNAL	0x80000000
#define SYSMODULE_ID_SYSTEM	0x40000000	/* from the *Internal syscalls */

#define SCE_SYSMODULE_ERROR_INVALID_VALUE	0x805a1000
#define SCE_SYSMODULE_ERROR_UNLOADED		0x805a1001
#define SCE_SYSMODULE_ERROR_FATAL		0x805a10ff
/* the same codes with bit 30 set (names unknown) */
#define SYSMODULE_ERROR_NOT_ALLOWED		(SCE_SYSMODULE_ERROR_INVALID_VALUE | 0x40000000)
#define SYSMODULE_ERROR_BAD_FLAGS		(0x805a10fe | 0x40000000)
#define SYSMODULE_ERROR_NOT_LOADED		(SCE_SYSMODULE_ERROR_UNLOADED | 0x40000000)	/* unload */

/* a user library */
typedef struct SysmoduleLib {
	const char *name;			/* NULL: none */
	SceUInt16 nameLen;			/* including the terminator */
	SceUInt16 flags;			/* SYSMODULE_LIB_* */
} SysmoduleLib;

#define SYSMODULE_LIB_VS0		0x0001	/* vs0:sys/external/ */
#define SYSMODULE_LIB_DEVONLY		0x0004	/* development mode only; dipsw 0xd2: other load flags */
#define SYSMODULE_LIB_RETAILONLY	0x0008
#define SYSMODULE_LIB_SHARED		0x0010	/* shared module */
#define SYSMODULE_LIB_APP		0x0020	/* app0:sce_module/ */
#define SYSMODULE_LIB_NONE		0x0040	/* (with no name: nothing to load) */
#define SYSMODULE_LIB_DIPSW_D4		0x0080
#define SYSMODULE_LIB_DIPSW_C2		0x0100

/* a module ID: its libraries, loaded last to first */
typedef struct SysmoduleEntry {
	const SceUInt8 *libs;
	SceUInt16 nlibs;
	SceUInt16 flags;			/* SYSMODULE_MOD_* */
} SysmoduleEntry;

#define SYSMODULE_MOD_PUBLIC		0x0001
#define SYSMODULE_MOD_SYSTEM		0x0002	/* system programs (or *Internal) */
#define SYSMODULE_MOD_NONGAME		0x0004	/* non-game programs (or system) */
#define SYSMODULE_MOD_PSMDEV		0x0008	/* PSM dev assistant (or system) */
#define SYSMODULE_MOD_SYSONLY		0x0010	/* system programs only */
#define SYSMODULE_MOD_DEVONLY		0x0020	/* development mode only */
#define SYSMODULE_MOD_OPT		0x0040	/* takes load / unload options */

#include "tables.h"

/* load / unload options (sceSysmodule*InternalWithArg) */
typedef struct SysmoduleOpt {
	SceSize args;
	void *argp;
	SceUInt32 flags;
	void *loadOpt;
	void *sharedLoadOpt;
	void *unloadOpt;
	void *sharedUnloadOpt;
	int *status;
} SysmoduleOpt;

/* per process (process local storage, 0x1000) */
typedef struct SysmoduleProc {
	struct {
		SceUInt32 ref;
		SceUID modid;			/* -1: nothing loaded (skipped) */
	} lib[SYSMODULE_LIBS];
	struct {
		SceUInt16 loaded;		/* load count of the module ID */
		SceUInt16 pos;			/* libraries done */
	} module[SYSMODULE_MODULES + SYSMODULE_INTERNAL];
	char path[SYSMODULE_PATH_MAX];
	SceUInt32 reserved[16];
	SceUID mutex;
	const char *prefix;			/* host0: prefix of an interrupted load */
	SysmoduleOpt opt;
	SceUID waiter;				/* thread whose load was interrupted */
} SysmoduleProc;

_Static_assert(offsetof(SysmoduleProc, module) == 0x3e8, "SysmoduleProc");
_Static_assert(offsetof(SysmoduleProc, path) == 0x5e8, "SysmoduleProc");
_Static_assert(offsetof(SysmoduleProc, mutex) == 0x668, "SysmoduleProc");
_Static_assert(offsetof(SysmoduleProc, waiter) == 0x690, "SysmoduleProc");

static const char s_host[] = "host0:sysmodule/";
static const char s_hostVs0[] = "host0:vs0/sys/external/";
static const char s_vs0[] = "vs0:sys/external/";
static const char s_app[] = "app0:sce_module/";

static int s_storage;

static int procCreateHandler(SceUID pid, ProcEventCreateParam *param, void *common);
static int procKillHandler(SceUID pid, ProcEventParam *param, void *common);
static int procExitHandler(SceUID pid, ProcEventParam *param, void *common);

static const SceKernelProcEventHandler s_procHandler = {
	.size = sizeof(SceKernelProcEventHandler),
	.create = procCreateHandler,
	.exit = procExitHandler,
	.kill = procKillHandler,
};

static inline int sysmoduleBlocked(int ret)
{
	return (SceUInt32)(ret - SCE_KERNEL_ERROR_THREAD_STOPPED) <= 1;
}

/* name unknown: load one library from prefix */
static int sysmoduleLoadLib(SysmoduleProc *proc, SceUInt32 *ref, const SysmoduleLib *lib,
			    const char *prefix, SceSize prefixLen)
{
	SceUInt32 flags, alt;
	SceUID pid, modid;
	int perm;

	proc->prefix = NULL;
	if (lib->name == NULL || lib->nameLen + prefixLen > SYSMODULE_PATH_MAX)
		return SCE_SYSMODULE_ERROR_FATAL;
	memcpy(proc->path, prefix, prefixLen);
	memcpy(proc->path + prefixLen, lib->name, lib->nameLen);
	if (lib->flags & SYSMODULE_LIB_APP) {
		alt = 0x9020;
		flags = 0x9000;
	} else {
		alt = 0x8020;
		flags = 0x8000;
	}
	if ((lib->flags & SYSMODULE_LIB_DEVONLY) &&
	    sceKernelCheckDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART))
		flags = alt;
	pid = sceKernelGetProcessId();
	perm = sceKernelSetThreadAccessLevel(0x80);
	if (perm < 0)
		return perm;
	if (!(lib->flags & SYSMODULE_LIB_SHARED))
		modid = SceModulemgrForDriver_9D953C22(pid, proc->path, proc->opt.args, proc->opt.argp,
							flags | proc->opt.flags, proc->opt.loadOpt,
							proc->opt.status);
	else
		modid = SceModulemgrForDriver_E2ADEF8D(pid, proc->path, proc->opt.args, proc->opt.argp,
							      flags | proc->opt.flags, proc->opt.sharedLoadOpt,
							      proc->opt.status);
	sceKernelSetThreadAccessLevel(perm);
	if (modid < 0) {
		if (sysmoduleBlocked(modid)) {
			proc->prefix = prefix;
			return modid;
		}
		SceDebugForDriver_391B74B7("Loading %s : Failed (0x%08x)\n", proc->path, modid);
		return modid;
	}
	ref[1] = modid;
	ref[0]++;
	return 0;
}

/* name unknown: the module ID's entry and the caller's state for it */
static int sysmoduleFind(SysmoduleProc **proc, SceUInt16 **state, const SysmoduleEntry **entry, SceUInt32 id)
{
	const SysmoduleEntry *e;
	SceUInt32 n = id & 0xffff, idx, flags;
	int ret;

	if (id & SYSMODULE_ID_INTERNAL) {
		idx = n + SYSMODULE_MODULES;
		if (idx >= SYSMODULE_MODULES + SYSMODULE_INTERNAL)
			return SYSMODULE_ERROR_NOT_ALLOWED;
		e = &s_internal[n];
	} else {
		if (n >= SYSMODULE_MODULES)
			return SYSMODULE_ERROR_NOT_ALLOWED;
		e = &s_modules[n];
		idx = n;
	}
	*entry = e;
	flags = e->flags;
	if (flags & SYSMODULE_MOD_DEVONLY) {
		if (!SceSblACMgrForDriver_E87D1777())
			return SYSMODULE_ERROR_NOT_ALLOWED;
		flags = e->flags;
	}
	if (flags & SYSMODULE_MOD_SYSONLY) {
		if (!sceSblACMgrIsSystemProgram(0))
			return SYSMODULE_ERROR_NOT_ALLOWED;
		flags = e->flags;
	}
	if (flags & SYSMODULE_MOD_NONGAME) {
		if ((id & SYSMODULE_ID_SYSTEM) || sceSblACMgrIsSystemProgram(0))
			goto allowed;
		flags = SceSblACMgrForDriver_6C5AB07F(0);
	} else if (flags & SYSMODULE_MOD_PSMDEV) {
		if ((id & SYSMODULE_ID_SYSTEM) || sceSblACMgrIsSystemProgram(0))
			goto allowed;
		flags = SceSblACMgrForDriver_C98D82EE(0);
	} else if (flags & SYSMODULE_MOD_SYSTEM) {
		if (id & SYSMODULE_ID_SYSTEM)
			goto allowed;
		flags = sceSblACMgrIsSystemProgram(0);
	} else {
		flags &= SYSMODULE_MOD_PUBLIC;
	}
	if (flags != 1)
		return SYSMODULE_ERROR_NOT_ALLOWED;
allowed:
	if (s_storage < 0)
		return s_storage;
	ret = SceProcessmgrForDriver_AF80F39C(0, s_storage, (void **)proc, 0);
	if (ret < 0)
		return ret;
	if (*proc == NULL)
		return SCE_SYSMODULE_ERROR_FATAL;
	if ((*proc)->mutex < 0)
		return (*proc)->mutex;
	*state = &(*proc)->module[idx].loaded;
	return 0;
}

/* name unknown: lock the process state, unless this thread is resuming an interrupted load */
static inline int sysmoduleLock(SysmoduleProc *proc)
{
	if (SceThreadmgrForDriver_20C228E4() && proc->waiter == sceKernelGetThreadId()) {
		proc->waiter = -1;
		return 0;
	}
	return sceKernelLockMutex(proc->mutex, 1, NULL);
}

static inline void sysmoduleSetOpt(SysmoduleProc *proc, const SysmoduleEntry *entry, const SysmoduleOpt *opt)
{
	if (opt != NULL && (entry->flags & SYSMODULE_MOD_OPT)) {
		proc->opt.args = opt->args;
		proc->opt.argp = opt->argp;
		proc->opt.flags = opt->flags;
		proc->opt.loadOpt = opt->loadOpt;
		proc->opt.sharedLoadOpt = opt->sharedLoadOpt;
		proc->opt.unloadOpt = opt->unloadOpt;
		proc->opt.sharedUnloadOpt = opt->sharedUnloadOpt;
		proc->opt.status = opt->status;
	} else
		memset(&proc->opt, 0, sizeof(proc->opt));
}

/* name unknown: libnp_message_padding, or nothing for a process of 0x1800000 or more */
static inline SceUInt32 sysmoduleLibAlt(SceUInt32 lib)
{
	SceSize size = 0;

	if (lib == SYSMODULE_LIBRARY_NP_MESSAGE_PADDING &&
	    SceProcessmgrForDriver_D141C076(sceKernelGetProcessId(), &size) == 0)
		lib = size >= 0x1800000 ? SYSMODULE_LIBRARY_NONE :
						    SYSMODULE_LIBRARY_NP_MESSAGE_PADDING;
	return lib;
}

/* the user PMU, for libperf (value: 1 on, 0 off) */
/* the 3.6x build has these messages compiled out */
#if FW_SINCE(0x04000000)
#define PMON_PRINTF(_line, _fmt, _ret)	SCE_DBG_PRINTF(0, (_line), _fmt, (_ret))
#else
#define PMON_PRINTF(_line, _fmt, _ret)	((void)(_ret))
#endif

static inline void sceKernelPerfArmPmonOpen(void)
{
	int ret;

	ret = SceProcessmgrForDriver_61B9B6FA(0, 0x11);
	if (ret != 0) {
		PMON_PRINTF(1348, "[Error] sceKernelSetInitialPMCR (ret = 0x%08x)\n", ret);
		return;
	}
	ret = sceKernelPMonSetControlRegister(0, 0x11);
	if (ret != 0) {
		PMON_PRINTF(1354, "[Error] sceKernelPMonSetControlRegister (ret = 0x%08x)\n", ret);
		return;
	}
	ret = SceProcessmgrForDriver_B1C3EFCA(0, 1);
	if (ret != 0) {
		PMON_PRINTF(1360, "[Error] sceKernelSetInitialPMUSERENR (ret = 0x%08x)\n", ret);
		return;
	}
	ret = sceKernelPMonSetUserEnableRegister(0, 1);
	if (ret != 0) {
		PMON_PRINTF(1367, "[Error] sceKernelPMonSetUserEnableRegister (ret = 0x%08x)\n", ret);
		return;
	}
	ret = sceKernelSetPMUSERENR(0, 1);
	if (ret != 0)
		PMON_PRINTF(1373, "[Error] sceKernelSetPMUSERENR (ret = 0x%08x)\n", ret);
}

static inline void sceKernelPerfArmPmonClose(void)
{
	int ret;

	ret = SceProcessmgrForDriver_61B9B6FA(0, 0);
	if (ret != 0) {
		PMON_PRINTF(1395, "[Error] sceKernelSetInitialPMCR (ret = 0x%08x)\n", ret);
		return;
	}
	ret = sceKernelPMonSetControlRegister(0, 0);
	if (ret != 0) {
		PMON_PRINTF(1401, "[Error] sceKernelPMonSetControlRegister (ret = 0x%08x)\n", ret);
		return;
	}
	ret = SceProcessmgrForDriver_B1C3EFCA(0, 0);
	if (ret != 0) {
		PMON_PRINTF(1407, "[Error] sceKernelSetInitialPMUSERENR (ret = 0x%08x)\n", ret);
		return;
	}
	ret = sceKernelPMonSetUserEnableRegister(0, 0);
	if (ret != 0) {
		PMON_PRINTF(1414, "[Error] sceKernelPMonSetUserEnableRegister (ret = 0x%08x)\n", ret);
		return;
	}
	ret = sceKernelSetPMUSERENR(0, 0);
	if (ret != 0)
		PMON_PRINTF(1420, "[Error] sceKernelSetPMUSERENR (ret = 0x%08x)\n", ret);
}

/* name unknown: whether a library is wanted on this unit */
static inline int sysmoduleLibWanted(const SysmoduleLib *lib)
{
	SceUInt32 flags = lib->flags;

	if (flags & SYSMODULE_LIB_DEVONLY) {
		if (!SceSblACMgrForDriver_E87D1777())
			return 0;
		flags = lib->flags;
	}
	if (flags & SYSMODULE_LIB_RETAILONLY) {
		if (SceSblACMgrForDriver_E87D1777())
			return 0;
		flags = lib->flags;
	}
	if (flags & SYSMODULE_LIB_DIPSW_D4) {
		if (!sceKernelCheckDipsw(DIPSW_USER_PA_MAPPING))
			return 0;
		flags = lib->flags;
	}
	if (flags & SYSMODULE_LIB_DIPSW_C2) {
		if (!sceKernelCheckDipsw(DIPSW_DECI))
			return 0;
		flags = lib->flags;
	}
	return flags != SYSMODULE_LIB_NONE || lib->name != NULL;
}

/* name unknown: load a library, from the host first when the debugger allows it */
static inline int sysmoduleLoadLibFrom(SysmoduleProc *proc, SceUInt32 *ref, const SysmoduleLib *lib)
{
	const char *prefix;
	int ret;

	if (SceQafMgrForDriver_B9770A13() && sceKernelCheckDipsw(DIPSW_HOST0_MODULES) &&
	    (sceKernelCheckDipsw(DIPSW_SYSTEM_DECI) || sceKernelCheckDipsw(DIPSW_DECI))) {
		prefix = proc->prefix;
		if (prefix == NULL || prefix == s_host) {
			ret = sysmoduleLoadLib(proc, ref, lib, s_host, sizeof(s_host) - 1);
			if (ret == 0 || ret == SCE_KERNEL_ERROR_THREAD_SUSPENDED)
				return ret;
			if (ret == SCE_KERNEL_ERROR_THREAD_STOPPED)
				return ret;
			prefix = proc->prefix;
			if (prefix == NULL)
				goto host_vs0;
		}
		if (prefix == s_hostVs0) {
host_vs0:
			ret = sysmoduleLoadLib(proc, ref, lib, s_hostVs0, sizeof(s_hostVs0) - 1);
			if (ret == 0 || ret == SCE_KERNEL_ERROR_THREAD_SUSPENDED)
				return ret;
			if (ret == SCE_KERNEL_ERROR_THREAD_STOPPED)
				return ret;
		}
	}
	if (lib->flags & SYSMODULE_LIB_APP)
		return sysmoduleLoadLib(proc, ref, lib, s_app, sizeof(s_app) - 1);
	if (lib->flags & SYSMODULE_LIB_VS0)
		return sysmoduleLoadLib(proc, ref, lib, s_vs0, sizeof(s_vs0) - 1);
	return SCE_SYSMODULE_ERROR_FATAL;
}

/* name unknown */
static int sysmoduleLoad(SceUInt32 id, const SysmoduleOpt *opt)
{
	SysmoduleProc *proc = NULL;
	SceUInt16 *state = NULL;
	const SysmoduleEntry *entry = NULL;
	const SceUInt8 *p;
	SceUInt32 t, idx, lib, *ref;
	int ret;

	t = sceSyscallEnter();
	ret = sysmoduleFind(&proc, &state, &entry, id);
	if (ret < 0) {
		sceSyscallLeave(t);
		return ret;
	}
	ret = sysmoduleLock(proc);
	if (ret < 0) {
		sceSyscallLeave(t);
		return ret;
	}
	sysmoduleSetOpt(proc, entry, opt);

	if (state[0] == 0) {
		if (entry->nlibs < state[1]) {
			ret = SCE_SYSMODULE_ERROR_FATAL;
			goto unlock;
		}
		p = entry->libs + (entry->nlibs - state[1]);
		while (entry->nlibs > state[1]) {
			idx = *--p;
			if (idx - 1 >= SYSMODULE_LIBS - 1) {
				ret = SCE_SYSMODULE_ERROR_FATAL;
				goto unlock;
			}
			lib = sysmoduleLibAlt(idx);
			ref = &proc->lib[idx].ref;
			if (ref[0] != 0) {
				ref[0]++;
			} else if (!sysmoduleLibWanted(&s_libs[lib])) {
				ref[1] = -1;
				ref[0]++;
			} else if (idx == SYSMODULE_LIBRARY_PERF) {
				/* libperf: the user PMU is enabled instead */
				sceKernelPerfArmPmonOpen();
				ref[1] = SYSMODULE_LIBRARY_PERF;
				ref[0]++;
			} else {
				ret = sysmoduleLoadLibFrom(proc, ref, &s_libs[lib]);
				if (ret < 0) {
					if (!sysmoduleBlocked(ret))
						goto unlock;
					proc->waiter = sceKernelGetThreadId();
					sceSyscallLeave(t);
					return ret;
				}
			}
			state[1]++;
		}
	}
	ret = 0;
	state[0]++;
unlock:
	sceKernelUnlockMutex(proc->mutex, 1);
	sceSyscallLeave(t);
	return ret;
}

/* name unknown */
static int sysmoduleUnload(SceUInt32 id, const SysmoduleOpt *opt)
{
	SysmoduleProc *proc = NULL;
	SceUInt16 *state = NULL;
	const SysmoduleEntry *entry = NULL;
	const SceUInt8 *p;
	SceUInt32 t, idx, lib, *ref;
	SceUID pid;
	int ret;

	t = sceSyscallEnter();
	ret = sysmoduleFind(&proc, &state, &entry, id);
	if (ret < 0) {
		sceSyscallLeave(t);
		return ret;
	}
	ret = sysmoduleLock(proc);
	if (ret < 0) {
		sceSyscallLeave(t);
		return ret;
	}
	sysmoduleSetOpt(proc, entry, opt);

	if (state[0] != 0) {
		if (--state[0] != 0) {
			ret = 0;
			goto unlock;
		}
	} else if (state[1] == 0) {
		ret = SYSMODULE_ERROR_NOT_LOADED;
		goto unlock;
	}
	if (entry->nlibs < state[1]) {
		state[0] = 1;
		ret = SCE_SYSMODULE_ERROR_FATAL;
		goto unlock;
	}
	p = entry->libs + (entry->nlibs - state[1]);
	ret = 0;
	while (state[1] != 0) {
		idx = *p++;
		if (idx - 1 >= SYSMODULE_LIBS - 1) {
			state[0]++;
			ret = SCE_SYSMODULE_ERROR_FATAL;
			goto unlock;
		}
		ref = &proc->lib[idx].ref;
		if (ref[0] == 0) {
			state[0]++;
			ret = SYSMODULE_ERROR_NOT_LOADED;
			goto unlock;
		}
		if (--ref[0] == 0) {
			if (idx == SYSMODULE_LIBRARY_PERF && ref[1] == SYSMODULE_LIBRARY_PERF) {
				sceKernelPerfArmPmonClose();
			} else if (ref[1] != (SceUInt32)-1) {
				pid = sceKernelGetProcessId();
				lib = sysmoduleLibAlt(idx);
				if (s_libs[lib].flags & SYSMODULE_LIB_SHARED)
					ret = SceModulemgrForDriver_02D3D0C1(pid, ref[1], proc->opt.args,
							proc->opt.argp, proc->opt.flags,
							proc->opt.sharedUnloadOpt, proc->opt.status);
				else
					ret = SceModulemgrForDriver_49A3EDC7(pid, ref[1], proc->opt.args,
							proc->opt.argp, proc->opt.flags,
							proc->opt.unloadOpt, proc->opt.status);
				if (ret < 0) {
					ref[0]++;
					state[0]++;
					if (!sysmoduleBlocked(ret))
						goto unlock;
					proc->waiter = sceKernelGetThreadId();
					sceSyscallLeave(t);
					return ret;
				}
			}
		}
		state[1]--;
	}
	ret = 0;
unlock:
	sceKernelUnlockMutex(proc->mutex, 1);
	sceSyscallLeave(t);
	return ret;
}

/* name unknown */
static int sysmoduleIsLoaded(SceUInt32 id)
{
	SysmoduleProc *proc = NULL;
	const SysmoduleEntry *entry;
	SceUInt32 t, n = id & 0xffff, idx, flags;
	int ret;

	t = sceSyscallEnter();
	if (id & SYSMODULE_ID_INTERNAL) {
		idx = n + SYSMODULE_MODULES;
		if (idx >= SYSMODULE_MODULES + SYSMODULE_INTERNAL)
			goto denied;
		entry = &s_internal[n];
	} else {
		if (n >= SYSMODULE_MODULES)
			goto denied;
		entry = &s_modules[n];
		idx = n;
	}
	flags = entry->flags;
	if (flags & SYSMODULE_MOD_DEVONLY) {
		if (!SceSblACMgrForDriver_E87D1777())
			goto denied;
		flags = entry->flags;
	}
	if (flags & SYSMODULE_MOD_SYSONLY) {
		if (!sceSblACMgrIsSystemProgram(0))
			goto denied;
		flags = entry->flags;
	}
	if (flags & SYSMODULE_MOD_NONGAME) {
		if (!(id & SYSMODULE_ID_SYSTEM) && !sceSblACMgrIsSystemProgram(0)) {
			if (SceSblACMgrForDriver_6C5AB07F(0) != 1)
				goto denied;
		}
	} else {
		if (flags & SYSMODULE_MOD_PSMDEV) {
			if ((id & SYSMODULE_ID_SYSTEM) || sceSblACMgrIsSystemProgram(0))
				goto allowed;
			flags = SceSblACMgrForDriver_C98D82EE(0);
		} else if (flags & SYSMODULE_MOD_SYSTEM) {
			if (id & SYSMODULE_ID_SYSTEM)
				goto allowed;
			flags = sceSblACMgrIsSystemProgram(0);
		} else {
			flags &= SYSMODULE_MOD_PUBLIC;
		}
		if (flags != 1)
			goto denied;
	}
allowed:
	ret = s_storage;
	if (s_storage < 0)
		goto out;
	ret = SceProcessmgrForDriver_AF80F39C(0, s_storage, (void **)&proc, 0);
	if (ret < 0)
		goto out;
	if (proc == NULL) {
		ret = SCE_SYSMODULE_ERROR_FATAL;
		goto out;
	}
	ret = proc->mutex;
	if (proc->mutex < 0)
		goto out;
	ret = sysmoduleLock(proc);
	if (ret < 0)
		goto out;
	ret = SCE_SYSMODULE_ERROR_UNLOADED;
	if (proc->module[idx].loaded != 0 && proc->module[idx].pos == entry->nlibs)
		ret = 0;
	sceKernelUnlockMutex(proc->mutex, 1);
	goto out;
denied:
	ret = SYSMODULE_ERROR_NOT_ALLOWED;
out:
	sceSyscallLeave(t);
	return ret;
}

/* -- process events ----------------------------------------------------- */

/* name unknown */
static inline void sysmoduleProcEnd(SceUID pid)
{
	SysmoduleProc *proc = NULL;

	if (s_storage >= 0 &&
	    SceProcessmgrForDriver_AF80F39C(pid, s_storage, (void **)&proc, 0) >= 0 &&
	    proc != NULL && proc->mutex >= 0) {
		sceKernelDeleteMutex(proc->mutex);
		proc->mutex = -1;
	}
}

#if !FW_SINCE(0x04000000)
static int procKillHandler(SceUID pid, ProcEventParam *param, void *common)
{
	sysmoduleProcEnd(pid);
	return 0;
}
#endif

static int procCreateHandler(SceUID pid, ProcEventCreateParam *param, void *common)
{
	SysmoduleProc *proc = NULL;

	if (s_storage >= 0 &&
	    SceProcessmgrForDriver_AF80F39C(pid, s_storage, (void **)&proc, 1) >= 0 &&
	    proc != NULL) {
		proc->mutex = sceKernelCreateMutex("SceSysmodule", 0x8000, 0, NULL);
		proc->waiter = -1;
	}
	return 0;
}

#if FW_SINCE(0x04000000)
static int procKillHandler(SceUID pid, ProcEventParam *param, void *common)
{
	sysmoduleProcEnd(pid);
	return 0;
}
#endif

static int procExitHandler(SceUID pid, ProcEventParam *param, void *common)
{
	sysmoduleProcEnd(pid);
	return 0;
}

int module_start(SceSize args, void *argp)
{
	s_storage = sceKernelRegisterKPLS("SceSysmodule", 0x1000);
	sceKernelRegisterProcEventHandler("SceSysmodule", &s_procHandler, NULL);
	return SCE_KERNEL_START_SUCCESS;
}

/* -- syscalls ----------------------------------------------------------- */

/* the user passes a 16-bit id in a full register: Sony truncates it here */

int sceSysmoduleLoadModule(SceUInt32 id)
{
	return sysmoduleLoad((SceUInt16)id, NULL);
}

int sceSysmoduleUnloadModule(SceUInt32 id)
{
	return sysmoduleUnload((SceUInt16)id, NULL);
}

int sceSysmoduleIsLoaded(SceUInt32 id)
{
	return sysmoduleIsLoaded((SceUInt16)id);
}

int sceSysmoduleLoadModuleInternal(SceUInt32 id)
{
	return sysmoduleLoad(id | SYSMODULE_ID_SYSTEM, NULL);
}

int sceSysmoduleUnloadModuleInternal(SceUInt32 id)
{
	return sysmoduleUnload(id | SYSMODULE_ID_SYSTEM, NULL);
}

int sceSysmoduleIsLoadedInternal(SceUInt32 id)
{
	return sysmoduleIsLoaded(id | SYSMODULE_ID_SYSTEM);
}

/* name unknown */
static int __attribute__((noinline)) sysmoduleLoadWithOpt(SceUInt32 id, SceSize args, void *argp, SceUInt32 flags, int *status)
{
	SysmoduleOpt opt;

	if (flags & ~0x10)
		return SYSMODULE_ERROR_BAD_FLAGS;
	opt.args = args;
	opt.argp = argp;
	opt.flags = flags;
	opt.loadOpt = NULL;
	opt.sharedLoadOpt = NULL;
	opt.unloadOpt = NULL;
	opt.sharedUnloadOpt = NULL;
	opt.status = status;
	return sysmoduleLoad(id | SYSMODULE_ID_SYSTEM, &opt);
}

/* name unknown */
static int __attribute__((noinline)) sysmoduleUnloadWithOpt(SceUInt32 id, SceSize args, void *argp, SceUInt32 flags, int *status)
{
	SysmoduleOpt opt;

	if (flags != 0)
		return SYSMODULE_ERROR_BAD_FLAGS;
	opt.args = args;
	opt.argp = argp;
	opt.flags = 0;
	opt.loadOpt = NULL;
	opt.sharedLoadOpt = NULL;
	opt.unloadOpt = NULL;
	opt.sharedUnloadOpt = NULL;
	opt.status = status;
	return sysmoduleUnload(id | SYSMODULE_ID_SYSTEM, &opt);
}

/* sceSysmodule*InternalWithArg option (user) */
typedef struct SysmoduleUserOpt {
	SceUInt32 flags;
	int *status;				/* user: receives the module's status */
	SceUInt32 reserved[2];
} SysmoduleUserOpt;

int sceSysmoduleLoadModuleInternalWithArg(SceUInt32 id, SceSize args, void *argp, const SysmoduleUserOpt *uopt)
{
	SysmoduleUserOpt opt;
	int *userStatus;
	int status, ret, ret2;

	ret = sceKernelCopyFromUser(&opt, uopt, sizeof(opt));
	if (ret < 0)
		return ret;
	userStatus = opt.status;
	if (userStatus == NULL)
		return sysmoduleLoadWithOpt(id, args, argp, opt.flags, NULL);
	status = 0;
	ret = sysmoduleLoadWithOpt(id, args, argp, opt.flags, &status);
	ret2 = sceKernelCopyToUser(userStatus, &status, sizeof(status));
	return ret2 < 0 ? ret2 : ret;
}

int sceSysmoduleUnloadModuleInternalWithArg(SceUInt32 id, SceSize args, void *argp, const SysmoduleUserOpt *uopt)
{
	SysmoduleUserOpt opt;
	int *userStatus;
	int status, ret, ret2;

	ret = sceKernelCopyFromUser(&opt, uopt, sizeof(opt));
	if (ret < 0)
		return ret;
	userStatus = opt.status;
	if (userStatus == NULL)
		return sysmoduleUnloadWithOpt(id, args, argp, opt.flags, NULL);
	status = 0;
	ret = sysmoduleUnloadWithOpt(id, args, argp, opt.flags, &status);
	ret2 = sceKernelCopyToUser(userStatus, &status, sizeof(status));
	return ret2 < 0 ? ret2 : ret;
}
