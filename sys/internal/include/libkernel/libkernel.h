/*
 * SceLibKernel (os0:us/libkernel.suprx): the C library functions it exports that the SDK does
 * not declare (Sony's SDK has only the memory functions, kernel/libkernel.h). The prototypes
 * are LibKernel's definitions: the string, character and number functions are sysmem's
 * (sysmem/strings.c, ctype.c, search.c, checked_strings.c, ...), compiled again under these
 * names, and the heaps are dlmalloc's mspaces.
 */

#pragma once

#include <stdarg.h>
#include <scetypes.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/modulemgr.h>
#include <modulemgr/module_info.h>

#ifdef __cplusplus
extern "C" {
#endif

/* sceClibMspaceMallocStats: a heap's size and use */
struct MspaceStats {
	SceSize capacity;
	SceSize footprint;
	SceSize peakInUse;
	SceSize currentInUse;
};

void sceClibAbort(void);

int sceClibLookCtypeTable(int value);
int sceClibTolower(int value);
int sceClibToupper(int value);

SceSize sceClibStrnlen(const char *string, SceSize count);
int sceClibStrcmp(const char *left, const char *right);
int sceClibStrncmp(const char *left, const char *right, SceSize count);
int sceClibStrncasecmp(const char *left, const char *right, SceSize count);
char *sceClibStrchr(const char *string, int value);
char *sceClibStrrchr(const char *string, int value);
char *sceClibStrstr(const char *string, const char *substring);
char *sceClibStrncpy(char *destination, const char *source, SceSize count);
char *sceClibStrncat(char *destination, const char *source, SceSize count);
SceSize sceClibStrlcpy(char *destination, const char *source, SceSize capacity);
SceSize sceClibStrlcat(char *destination, const char *source, SceSize capacity);
long long sceClibStrtoll(const char *string, char **end, int base);
void *sceClibMemcpy_safe(void *destination, const void *source, SceSize size);

/* the _Chk forms: capacity is the destination's size; an overrun stops the process */
char *sceClibStrcpyChk(char *destination, const char *source, SceSize capacity);
char *sceClibStrncpyChk(char *destination, const char *source, SceSize count, SceSize capacity);
char *sceClibStrcatChk(char *destination, const char *source, SceSize capacity);
char *sceClibStrncatChk(char *destination, const char *source, SceSize count, SceSize capacity);
SceSize sceClibStrlcpyChk(char *destination, const char *source, SceSize count, SceSize capacity);
SceSize sceClibStrlcatChk(char *destination, const char *source, SceSize count, SceSize capacity);

int sceClibPrintf(const char *format, ...);
int sceClibVprintf(const char *format, va_list arguments);
int sceClibDprintf(SceUID descriptor, const char *format, ...);
int sceClibVdprintf(SceUID descriptor, const char *format, va_list arguments);
int sceClibSnprintf(char *destination, SceSize size, const char *format, ...);
int sceClibVsnprintf(char *destination, SceSize size, const char *format, va_list arguments);
int sceClibSnprintfChk(char *destination, SceSize size, int flags, SceSize destinationSize,
		       const char *format, ...);
int sceClibVsnprintfChk(char *destination, SceSize size, int flags, SceSize destinationSize,
			const char *format, va_list arguments);

void *sceClibMspaceCreate(void *base, SceSize capacity);
int sceClibMspaceDestroy(void *mspace);
void *sceClibMspaceMalloc(void *mspace, SceSize bytes);
void *sceClibMspaceCalloc(void *mspace, SceSize count, SceSize size);
void *sceClibMspaceRealloc(void *mspace, void *pointer, SceSize bytes);
void *sceClibMspaceReallocalign(void *mspace, void *pointer, SceSize bytes, SceSize alignment);
void *sceClibMspaceMemalign(void *mspace, SceSize alignment, SceSize bytes);
void sceClibMspaceFree(void *mspace, void *pointer);
int sceClibMspaceMallocStats(void *mspace, struct MspaceStats *stats);
int sceClibMspaceMallocStatsFast(void *mspace, struct MspaceStats *stats);
SceSize sceClibMspaceMallocUsableSize(void *pointer);
SceBool sceClibMspaceIsHeapEmpty(void *mspace);

/* the calling thread's TLS word at index */
void *sceKernelGetTLSAddr(SceUInt32 index);

/* the module holding address */
int sceKernelGetModuleInfoByAddr(const void *address, SceKernelModuleInfo *info);
SceUID sceKernelOpenModule(const char *path, SceSize size, void *argument, SceUInt flags,
			   const SceKernelLoadModuleOpt *option, int *status);
int sceKernelCloseModule(SceUID id, SceSize size, void *argument, SceUInt flags,
			 const SceKernelUnloadModuleOpt *option, int *status);

/* sceKernelCreateMsgPipe with the caller's address, which the kernel records */
SceUID sceKernelCreateMsgPipeWithLR(const char *name, SceUInt32 type, SceUInt32 attributes,
				    SceSize size, const SceKernelMsgPipeOptParam *option,
				    void *caller);

#ifdef __cplusplus
}
#endif
