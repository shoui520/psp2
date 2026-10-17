/*
 * SceSysLibTrace (os0:kd/syslibtrace.skprx): system call tracing of exported
 * libraries, and the kernel's NID -> symbol name table.
 */

#pragma once

#include <psp2kern/types.h>
#include <threadmgr/threadmgr.h>

/* SCE library export record (the module's export table entry) */
typedef struct SceModuleExports {
	SceUInt8 size;
	SceUInt8 auxattribute;
	SceUInt16 version;
	SceUInt16 attribute;
	SceUInt16 numFunctions;
	SceUInt16 numVars;
	SceUInt16 numTlsVars;
	SceUInt8 hashinfo;		/* low nibble: funcs, high: vars */
	SceUInt8 hashinfotls;
	SceUInt8 reserved;
	SceUInt8 nidaltsets;
	SceUInt32 libraryNid;
	const char *libraryName;
	const SceUInt32 *nidTable;
	void *const *entryTable;
} SceModuleExports;

#define SCE_LIBRARY_ATTR_SYSCALL_EXPORT	0x4000

/*
 * The module manager's entry for an exported library, as handed out by
 * ksceKernelGetLibEntCBListForSyslibtrace (Modulemgr's LibEntCB).
 */
typedef struct SceSyslibtraceLibEntry {
	struct SceSyslibtraceLibEntry *next;
	struct SceSyslibtraceLibEntry *older;	/* other versions of the same library */
	const SceModuleExports *exports;
	SceInt16 syscallBase;		/* syscall number of the first function */
	SceUInt16 traceFlags;		/* SCE_SYSLIBTRACE_LIB_* */
	int clientCount;
	void *clients;
	SceUID guid;
	SceUID puid;
	void *cb;			/* the exporting module */
	void *enterArg;			/* passed to the enter callback, set by the library callbacks */
	void *returnArg;		/* passed to the return callback */
} SceSyslibtraceLibEntry;

#define SCE_SYSLIBTRACE_LIB_TARGET	0x100	/* selected by sceSyslibtraceSetTargetSyscallLib */
#define SCE_SYSLIBTRACE_LIB_REGISTERED	0x200	/* seen by the register callback */
#define SCE_SYSLIBTRACE_LIB_TRACING	0x400	/* syscalls redirected to the trampoline */

#define SCE_SYSLIBTRACE_ALL_LIBS	0xFFFFFFFF	/* library NID that selects every library */

typedef int (*SceSyslibtraceSyscallCallback)(SceUInt32 libraryNid, SceUInt32 nid, SceUInt32 index,
					     void *arg, SceUInt32 r0, SceUInt32 r1, SceUInt32 r2,
					     SceUInt32 r3);
typedef int (*SceSyslibtraceLibCallback)(SceSyslibtraceLibEntry *lib, void **enterArg,
					 void **returnArg);

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

/* the traced function behind each syscall number (handed to the module manager) */
typedef struct SceSyslibtraceSyscallInfo {
	void *entry;
	SceUInt32 libraryNid;
	SceUInt32 nid;
	SceUInt32 index;		/* function index in the library */
	void *enterArg;
	void *returnArg;
} SceSyslibtraceSyscallInfo;

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
int sceSyslibtraceSetTargetSyscallLib(SceUInt32 libraryNid);
int sceSyslibtraceClearTargetSyscallLib(SceUInt32 libraryNid);
int sceSyslibtraceStartSyscallTrace(void);
int sceSyslibtraceStopSyscallTrace(void);

/* names unknown */
int SceSyslibtraceForKernel_F9CB8BA9(const SceSyslibtraceCallbacks *callbacks, SceUInt32 flags);	/* set callbacks */
int SceSyslibtraceForKernel_BD326721(SceUInt32 flags);			/* clear callbacks */
int SceSyslibtraceForKernel_7CC73CDA(SceUInt32 syscall, SceUInt32 *nid);	/* NID behind a syscall */
SceSize SceNidsymtblForDriver_138A9106(const SceNidsymtblEntry **table);	/* the table */
int SceNidsymtblForDriver_985E2935(SceUInt32 nid, const char **name);	/* name of a NID */

/* imports */
/* ksceKernelGetLibEntCBListForSyslibtrace: <psp2kern/kernel/modulemgr.h> (list of SceSyslibtraceLibEntry pointers) */
/* names unknown: hand the syscall table and trampoline to the module manager, the NID lookup to sysroot */
int SceModulemgrForKernel_B23286B8(SceSyslibtraceSyscallInfo *info, void *trampoline);
void SceSysrootForKernel_B39CD708(int (*lookup)(SceUInt32 nid, const char **name));
