#pragma once

#include <stddef.h>
#include <stdint.h>
#include <psp2kern/types.h>

/*
 * The debugger's table (deci4p_dbgp registers it, NID 3999F917): the process
 * and module events again, and the debugger's queries. Sony's handler names
 * where its records give them (process_handler.c, dbgp.c).
 */
typedef struct SysrootDbgpOps {
	SceSize size;
	int (*processStart)(SceUID parentPid, SceUID pid, SceUInt64 time);
	int (*processExit)(SceUID pid, int exitStatus, SceUInt64 time);
	int (*processKill)(SceUID pid, SceUInt64 time);
	/* processHandlerSpontaneousExitHandler: status 0x106..0x10a */
	int (*processSpontaneousExit)(SceUID pid, SceUInt32 status, SceUInt64 time);
	int (*processExitToSpawn)(SceUID pid, SceUID spawnPid, SceUInt64 time);
	int (*exitSpawn)(SceUID pid, SceUID spawnPid, SceUID prevPid, uint64_t time);
	int (*processSuspend)(SceUID pid);
	int (*processResume)(SceUID pid);
	int (*appAbort)(SceUID pid);		/* appmgr's abort handler, debugged process */
	int (*moduleStart)(SceUID pid, SceUID modid, SceUInt64 time);
	int (*moduleStop)(SceUID pid, SceUID modid, SceUInt64 time);
	int (*isDebugTarget)(SceUID pid);	/* gates the events above */
	int (*isPsButtonCaptured)(void);	/* power: 1 keeps the PS press from the system */
	int (*swBkptGetOriginalOpcode)(SceUID pid, void *address, SceUInt32 *opcode);
	/* modulemgr's stub linker wrote an instruction */
	int (*codeModified)(SceUID pid, SceUInt32 address, SceUInt32 instruction);
	int (*getPrxDebugFlag)(SceUID pid, SceUInt32 *flag);
	int (*suspendProcessAndWaitResume)(SceUID pid, SceUID modid, SceUInt32 stopReason,
					   SceUInt64 time);
	int (*isProcessAttached)(SceUID pid);
	int (*enableWatchpoint)(const struct SysrootDbgpOps *ops);
	int (*disableWatchpoint)(const struct SysrootDbgpOps *ops);
	/* dbgpUpdateUlobjmgrFlagForDbgp: through ulobjmgr, which adds the flag word */
	int (*updateUlobjmgrFlagForDbgp)(void *dbgp, void *process, SceUInt32 set,
					 SceUInt32 clear, SceUInt32 *flag);
	/* dbgpUpdateUlobjmgrFlagForLibfiber: the flag word of an attached process */
	int (*updateUlobjmgrFlagForLibfiber)(SceUID pid, SceUInt32 *flag);
} SysrootDbgpOps;

_Static_assert(sizeof(SysrootDbgpOps) == 0x5c, "Sysroot debugger table size");
_Static_assert(offsetof(SysrootDbgpOps, exitSpawn) == 0x18,
	       "Sysroot debugger ExitSpawn callback offset");
