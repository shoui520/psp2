#ifndef SCE_MODULE_LIBRARY_H
#define SCE_MODULE_LIBRARY_H

#include <stddef.h>
#include <scetypes.h>

/* a library's or a function's NID */
typedef SceUInt32 SceNID;

/* sceKernelGetLibraryInfoByNID: parallel function and variable export tables. */
typedef struct SceKernelLibraryInfo {
	SceSize size;
	SceUInt16 version;
	SceUInt16 attr;
	SceNID libnid;
	const char *libname;
	SceUInt16 nfunc;
	SceUInt16 nvar;
	SceNID *nidtable;
	void **entrytable;
} SceKernelLibraryInfo;

_Static_assert(sizeof(SceKernelLibraryInfo) == 0x1c, "library info ABI");

/* exported library entry (0x20 bytes) */
typedef struct SceModuleExports {
	SceUInt8 size;
	SceUInt8 auxattr;
	SceUInt16 version;
	SceUInt16 attr;
	SceUInt16 nfunc;
	SceUInt16 nvar;
	SceUInt16 ntls;
	SceUInt8 hashinfo;		/* low nibble: funcs, high: vars */
	SceUInt8 hashinfotls;
	SceUInt8 reserved;
	SceUInt8 nidaltsets;
	SceNID libnid;
	const char *libname;
	SceNID *nidtable;
	void **entrytable;
} SceModuleExports;

/* LibEntCB.flags */
#define LIBENT_FLAG_SHARED		0x0004	/* entries are trusted (copied module) */
#define LIBENT_FLAG_TRACE_TARGET	0x0100	/* syslibtrace: SCE_SYSLIBTRACE_LIB_TARGET */
#define LIBENT_FLAG_TRACING		0x0400	/* syslibtrace: SCE_SYSLIBTRACE_LIB_TRACING */

/* an exported library; sceKernelGetLibEntCBListForSyslibtrace hands them out */
struct LibStubCB;
struct SceModuleCB;

typedef struct LibEntCB {
	struct LibEntCB *next;		/* LibraryDB.libList */
	struct LibEntCB *older;		/* other versions of the same library */
	SceModuleExports *exports;
	SceInt16 syscallIndex;
	SceUInt16 flags;
	int clientCount;
	struct LibStubCB *clients;
	SceUID guid;
	SceUID puid;
	struct SceModuleCB *cb;
	void *enterArg;			/* syslibtrace: its enter and return callbacks' arguments */
	void *returnArg;
} LibEntCB;

_Static_assert(offsetof(LibEntCB, enterArg) == 0x24, "LibEntCB.enterArg");

/* one traced syscall (tool mode): the table SceModulemgrForKernel_B23286B8 installs */
typedef struct SyscallTraceInfo {
	void *func;
	SceNID libnid;
	SceNID funcnid;
	SceUInt32 index;		/* in the library */
	void *enterArg;			/* LibEntCB.enterArg */
	void *returnArg;
} SyscallTraceInfo;

#endif
