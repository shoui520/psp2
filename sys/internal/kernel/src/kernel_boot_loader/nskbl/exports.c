/*
 * SceKblForKernel: the boot loader's one library, which psp2bootconfig links to
 * (sceKernelLibraryDBSetPsp2bootcfg). nskbl.ld places the library entry at
 * __sce_libent_top, then its name, NIDs, NID hash and entries.
 */

#include <scetypes.h>
#include <kernel_boot_loader/kbl.h>
#include <modulemgr/module_library.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/dipsw.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/sysroot.h>
#include "../../sysmem/sysmem_internal.h"

/* kbl_main.c and archive_ns.c */
int KblLoadKernelModules(const char *const *names, SceUID *modid, int count, int shared);
int KblBootModulesUnlocked(void);
int KblMainStopSm(void);
void sceSdStandaloneInit(void);
int sceSdStandaloneTerm(void);

#define LIBRARY_NFUNC	29

static const char libraryName[]
	__attribute__((section(".sceExport.rodata.name"), aligned(4))) = "SceKblForKernel";

/* in ascending order, as the hash below indexes them */
static const SceNID libraryNids[LIBRARY_NFUNC]
	__attribute__((section(".sceExport.rodata.nid"), aligned(4))) = {
	0x08E9FAEB, 0x13A5ABEF, 0x161D6FCC, 0x1DB28F02, 0x261F2747, 0x314AA770,
	0x5945F065, 0x6D7A1F18, 0x752E7EEC, 0x79241ACF, 0x807B4437, 0x838466E9,
	0x8A416887, 0x943E7537, 0x9A92436E, 0x9B868276, 0x9F4F3F98, 0xA7BD4417,
	0xB506A10E, 0xB6C9ACF1, 0xB6D00D6D, 0xC011935A, 0xC3DDDE15, 0xC7B77991,
	0xC8F4DE71, 0xCE94F329, 0xD3A516D5, 0xDDB3A1A8, 0xF7AF8690,
};

/* hashinfo 2: for each top nibble of a NID, the index of its first NID */
static const SceUInt16 libraryNidHash[16]
	__attribute__((section(".sceExport.rodata.hash"), aligned(4), used)) = {
	0, 1, 4, 5, 6, 6, 7, 8, 10, 13, 17, 18, 21, 26, 28, 28,
};

static const void *const libraryEntries[LIBRARY_NFUNC]
	__attribute__((section(".sceExport.rodata.ent"), aligned(4))) = {
	SerialDebugPutchar,				/* 08E9FAEB */
	SceDebugForDriver_391B74B7,			/* 13A5ABEF */
	sceKernelSysrootProcessmgrStart2,		/* 161D6FCC */
	sceKernelSysrootThreadMgrStartAfterProcess,	/* 1DB28F02 */
	sceSdStandaloneTerm,				/* 261F2747 */
	sceKernelSysrootCorelockUnlock,			/* 314AA770 */
	SceSblAIMgrForDriver_F4B98F66,			/* 5945F065 */
	KblLoadKernelModules,				/* 6D7A1F18 */
	sceKernelPrintfLevel,				/* 752E7EEC */
	KblMainStopSm,					/* 79241ACF */
	sceKernelSysrootCorelockLock,			/* 807B4437 */
	SceSblAIMgrForDriver_4273B97B,			/* 838466E9 */
	SceSblAIMgrForDriver_D78B04A2,			/* 8A416887 */
	SceSblAIMgrForDriver_3B638885,			/* 943E7537 */
	sceKernelBootBootModules,			/* 9A92436E */
	SceDebugForKernel_8D474850,			/* 9B868276 */
	KblBootModulesUnlocked,				/* 9F4F3F98 */
	SceSblAIMgrForDriver_71608CA3,			/* A7BD4417 */
	sceKernelCpuId,					/* B506A10E */
	SceSblAIMgrForDriver_274663A0,			/* B6C9ACF1 */
	SceSblAIMgrForDriver_C6E83F34,			/* B6D00D6D */
	SceKblForKernel_C011935A,			/* C011935A */
	SceSblAIMgrForDriver_6D5A3FC9,			/* C3DDDE15 */
	sceKernelSysrootIofilemgrStart,			/* C7B77991 */
	sceKernelCheckDipsw,				/* C8F4DE71 */
	SceQafMgrForDriver_382C71E8,			/* CE94F329 */
	SceSysrootForDriver_46E72428,			/* D3A516D5 */
	SceKblForKernel_DDB3A1A8,			/* DDB3A1A8 */
	sceSdStandaloneInit,				/* F7AF8690 */
};

const SceModuleExports g_kblLibrary __attribute__((section(".sceLib.ent"), used)) = {
	.size = sizeof(SceModuleExports),
	.version = 1,
	.attr = 0x0001,
	.nfunc = LIBRARY_NFUNC,
	.hashinfo = 2,
	.libnid = 0xD0FC2991,
	.libname = libraryName,
	.nidtable = (SceNID *)libraryNids,
	.entrytable = (void **)libraryEntries,
};
