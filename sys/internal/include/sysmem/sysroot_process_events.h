#pragma once

#include <stddef.h>
#include <stdint.h>
#include <psp2kern/types.h>

/* Process events (deci4p_loadp registers the table, NID 0F07C3FC; processmgr and modulemgr raise them). */
typedef struct SysrootProcessEventOps {
	SceSize size;
	int (*processStart)(SceUID parentPid, SceUID pid, int flags, uint64_t time);
	int (*processExit)(SceUID pid, int exitStatus, uint64_t time);
	int (*processKill)(SceUID pid, uint64_t time);
	int (*loadProgress)(SceUID pid, SceUID modid, int percent, const char *path);	/* SELF decryption */
	int (*moduleStart)(SceUID pid, SceUID modid, uint64_t time);
	int (*moduleStop)(SceUID pid, SceUID modid, uint64_t time);
	int (*processQuery)(SceUID pid);			/* no caller in os0 */
	int (*processExitToSpawn)(SceUID pid, SceUID spawnPid, uint64_t time);
	int (*exitSpawn)(SceUID pid, SceUID spawnPid, SceUID prevPid, uint64_t time);
} SysrootProcessEventOps;

#ifndef __cplusplus
_Static_assert(sizeof(SysrootProcessEventOps) == 0x28,
	       "Sysroot process-event table size");
_Static_assert(offsetof(SysrootProcessEventOps, exitSpawn) == 0x24,
	       "Sysroot process-event ExitSpawn callback offset");
#endif
