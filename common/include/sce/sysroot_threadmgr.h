#pragma once

#include <stddef.h>
#include <stdint.h>
#include <psp2kern/types.h>

/* Thread-manager services registered with Sysroot by NID 26458702. */
typedef struct SysrootThreadmgrOps {
	SceSize size;
	SceUID (*getThreadId)(void);
	int (*isUserModeThread)(void);
	int (*getThreadAccessLevel)(void);
	int (*getCurrentThreadAttr)(uint32_t *attr);	/* sceKernelSysrootGetCurrentThreadAttr */
	SceUID (*getProcessId)(void);
	uint32_t (*getCurrentProcessCB)(void);		/* sceKernelSysrootGetCurrentProcessCB */
	void *(*getCurrentAddressSpaceCB)(void);
	void *(*getCurrentUIDEntryHeapCB)(void);
	/* Entry addresses: callback argument contracts remain unreviewed. */
	int (*returnFromExcpToThread)(void);
	int (*threadMgrStartAfterProcess)(void);
	int (*trapThreadAfterSyscall)(void);
} SysrootThreadmgrOps;

_Static_assert(sizeof(SysrootThreadmgrOps) == 0x30,
	       "Sysroot thread-manager service table size");
_Static_assert(offsetof(SysrootThreadmgrOps, getThreadAccessLevel) == 0x0c,
	       "Sysroot thread access-level callback slot");
