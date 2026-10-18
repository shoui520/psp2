#ifndef SCE_MODULE_LIBRARY_H
#define SCE_MODULE_LIBRARY_H

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

#endif
