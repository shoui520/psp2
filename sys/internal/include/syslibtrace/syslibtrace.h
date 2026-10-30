/*
 * SceSysLibTrace (os0:kd/syslibtrace.skprx): system call tracing of exported
 * libraries, and the kernel's NID -> symbol name table.
 */

#pragma once

#include <scetypes.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysroot.h>
#include <modulemgr/module_library.h>

#define SCE_LIBRARY_ATTR_SYSCALL_EXPORT	0x4000

/* LibEntCB.flags */
#define SCE_SYSLIBTRACE_LIB_TARGET	0x100	/* selected by sceSyslibtraceSetTargetSyscallLib */
#define SCE_SYSLIBTRACE_LIB_REGISTERED	0x200	/* seen by the register callback */
#define SCE_SYSLIBTRACE_LIB_TRACING	0x400	/* syscalls redirected to the trampoline */

#define SCE_SYSLIBTRACE_ALL_LIBS	0xFFFFFFFF	/* library NID that selects every library */

typedef int (*SceSyslibtraceSyscallCallback)(SceUInt32 libraryNid, SceUInt32 nid, SceUInt32 index,
					     void *arg, SceUInt32 r0, SceUInt32 r1, SceUInt32 r2,
					     SceUInt32 r3);
typedef int (*SceSyslibtraceLibCallback)(LibEntCB *lib, void **enterArg, void **returnArg);

/* argument of SceSyslibtraceForKernel_F9CB8BA9 */
typedef struct SceSyslibtraceCallbacks {
	SceSize size;					/* not checked */
	SceSyslibtraceSyscallCallback enter;
	SceSyslibtraceSyscallCallback leave;
	SceSyslibtraceLibCallback registerLib;
	SceSyslibtraceLibCallback deregisterLib;
	SceSyslibtraceLibCallback setTarget;
	SceSyslibtraceLibCallback clearTarget;
} SceSyslibtraceCallbacks;

#define SCE_SYSLIBTRACE_NUM_SYSCALLS	0x1000

/* SceNidsymtblForDriver: one row of the NID -> name table */
typedef struct SceNidsymtblEntry {
	SceUInt32 flags;		/* SCE_NIDSYMTBL_* */
	SceUInt32 nid;
	const char *name;
} SceNidsymtblEntry;

/* the name may be looked up without system application debug permission */
#define SCE_NIDSYMTBL_PUBLIC	0x1

int sceSyslibtraceCallbackRegisterLibs(SceUInt32 flags);
int sceSyslibtraceCallbackDeregisterLibs(SceUInt32 flags);
/*
 * the option of the trace controls: DTrace passes its size, syslibtrace does not
 * read it
 */
typedef struct SceSyslibtraceOpt {
	SceSize size;
} SceSyslibtraceOpt;

int sceSyslibtraceSetTargetSyscallLib(SceUInt32 libraryNid, const SceSyslibtraceOpt *opt);
int sceSyslibtraceClearTargetSyscallLib(SceUInt32 libraryNid, const SceSyslibtraceOpt *opt);
int sceSyslibtraceStartSyscallTrace(const SceSyslibtraceOpt *opt);
int sceSyslibtraceStopSyscallTrace(const SceSyslibtraceOpt *opt);

/* names unknown */
int SceSyslibtraceForKernel_F9CB8BA9(const SceSyslibtraceCallbacks *callbacks, SceUInt32 flags);	/* set callbacks */
int SceSyslibtraceForKernel_BD326721(SceUInt32 flags);			/* clear callbacks */
int SceSyslibtraceForKernel_7CC73CDA(SceUInt32 syscall, SceUInt32 *nid);	/* NID behind a syscall */
SceSize SceNidsymtblForDriver_138A9106(const SceNidsymtblEntry **table);	/* the table */
int SceNidsymtblForDriver_985E2935(SceUInt32 nid, const char **name);	/* name of a NID */

