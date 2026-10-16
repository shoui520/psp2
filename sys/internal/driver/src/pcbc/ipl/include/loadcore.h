/*
 * loadcore structures shared with the PSP kernel the IPL starts.
 */
#ifndef LOADCORE_H
#define LOADCORE_H

#include "ipl.h"

typedef int SceUID;

/* a module the kernel loads at boot (pspbtcnf.bin entry) */
typedef struct SceLoadCoreBootModuleInfo {
	char	*modPath;		/* 0x00 */
	void	*modBuf;		/* 0x04 */
	SceSize	modSize;		/* 0x08 */
	s32	reserved;		/* 0x0C */
	u32	attr;			/* 0x10 */
	s32	bootData;		/* 0x14 */
	u32	argSize;		/* 0x18 */
	SceUID	argPartId;		/* 0x1C */
} SceLoadCoreBootModuleInfo;

/* memory the boot loaders hand over, protected until the kernel takes it */
typedef struct SceLoadCoreProtectInfo {
	u32	addr;			/* 0x00 */
	SceSize	size;			/* 0x04 */
	u32	attr;			/* 0x08 */
	SceUID	partId;			/* 0x0C */
	s32	reserved[3];		/* 0x10 */
} SceLoadCoreProtectInfo;

typedef struct SceLoadCoreBootInfo {
	void	*memBase;		/* 0x00 */
	SceSize	memSize;		/* 0x04 */
	u32	loadedModules;		/* 0x08 */
	u32	numModules;		/* 0x0C */
	SceLoadCoreBootModuleInfo *modules;	/* 0x10 */
	s32	reserved0;		/* 0x14 */
	u32	moduleApiDefaultMode;	/* 0x18: low byte controls default API 0x210 */
	s32	numProtects;		/* 0x1C */
	SceLoadCoreProtectInfo *protects;	/* 0x20 */
	SceUID	modProtId;		/* 0x24 */
	SceUID	modArgProtId;		/* 0x28 */
	s32	model;			/* 0x2C */
	u32	buildVersion;		/* 0x30 */
	s32	reserved1;		/* 0x34 */
	char	*configFile;		/* 0x38 */
	u32	bootMode;		/* 0x3C */
	u32	bootArg0;		/* 0x40 */
	u32	bootArg1;		/* 0x44 */
	void	*self;			/* 0x48 */
	u32	reserved2[13];		/* 0x4C */
} SceLoadCoreBootInfo;

typedef struct SceLoadCoreExecFileInfo {
	u32	reserved0;		/* 0x00 */
	u32	modeAttribute;		/* 0x04 */
	u32	apiType;		/* 0x08 */
	u32	reserved1;		/* 0x0C */
	SceSize	execSize;		/* 0x10 */
	SceSize	maxAllocSize;		/* 0x14 */
	SceUID	decompressionMemId;	/* 0x18 */
	u8	*fileBase;		/* 0x1C */
	u32	elfType;		/* 0x20 */
	u8	*topAddr;		/* 0x24 */
	u32	entryAddr;		/* 0x28 */
	u32	reserved2;		/* 0x2C */
	SceSize	largestSegSize;		/* 0x30 */
	SceSize	textSize;		/* 0x34 */
	SceSize	dataSize;		/* 0x38 */
	SceSize	bssSize;		/* 0x3C */
	u32	partitionId;		/* 0x40 */
	u32	isKernelMod;		/* 0x44 */
	u32	isDecrypted;		/* 0x48 */
	u32	moduleInfoOffset;	/* 0x4C */
	void	*moduleInfo;		/* 0x50 */
	u32	isCompressed;		/* 0x54 */
	u16	modInfoAttribute;	/* 0x58 */
	u16	execAttribute;		/* 0x5A */
	SceSize	decSize;		/* 0x5C */
	u32	isDecompressed;		/* 0x60 */
	u32	isSignChecked;		/* 0x64 */
	u32	reserved3;		/* 0x68 */
	SceSize	overlapSize;		/* 0x6C */
	void	*exportsInfo;		/* 0x70 */
	SceSize	exportsSize;		/* 0x74 */
	void	*importsInfo;		/* 0x78 */
	SceSize	importsSize;		/* 0x7C */
	void	*strtabOffset;		/* 0x80 */
	u32	numSegments;		/* 0x84 */
	u32	segmentAddr[4];		/* 0x88 */
	SceSize	segmentSize[4];		/* 0x98 */
	SceUID	memBlockId;		/* 0xA8 */
	u32	segmentAlign[4];	/* 0xAC */
	u32	maxSegAlign;		/* 0xBC */
} SceLoadCoreExecFileInfo;

/* kernel services the boot loader calls through the kernel's own tables */
typedef struct SceLoadCoreBootServices SceLoadCoreBootServices;

#endif
