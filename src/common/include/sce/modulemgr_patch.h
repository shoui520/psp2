/*
 * The NID patches the process manager hands to sceKernelLoadProcessImage: for
 * a title, the libraries whose imports link to other NIDs.
 */

#pragma once

#include <psp2kern/types.h>

typedef struct ModulemgrNidPatch {
	SceNID from;
	SceNID to;
} ModulemgrNidPatch;

typedef struct ModulemgrLibraryPatch {
	const char *libname;
	SceUInt32 reserved;
	SceSize count;
	const ModulemgrNidPatch *patch;
} ModulemgrLibraryPatch;

typedef struct ModulemgrTitlePatch {
	const char *titleId;
	SceUInt32 reserved;
	SceSize count;
	const ModulemgrLibraryPatch *library;
} ModulemgrTitlePatch;
