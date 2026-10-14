/* Private loadcore interfaces used by the IPL boot path. */
#ifndef IPL_LOADELF_BOOT_H
#define IPL_LOADELF_BOOT_H

#include "loadcore.h"
#include "psp_header.h"
#include <elf.h>

/* Temporary segment table owned by the IPL's ELF loader. */
typedef struct SceLoadCoreBootSegments {
	u8 *destination[4];
	u8 *source[4];
	SceSize fileSize[4];
	u32 alignment[4];
} SceLoadCoreBootSegments;

typedef struct SceLoadCoreCompressedRelocationHeader {
	u16 formatVersion;
	u8 flagBits;
	u8 typeBits;
} SceLoadCoreCompressedRelocationHeader;

u32 GetRelocationBits(u32 value, u32 shift, u32 bits);
s32 ShiftRelocationHalfword(u32 value, u32 shift);
u32 MergeRelocationHalfwords(u32 high, u32 low, u32 shift);

int setGetLengthFunc(int (*function)(u8 *file, SceSize size,
				   SceSize *newSize));
int setPrepareGetLengthFunc(int (*function)(u8 *buffer, SceSize size));
int setMaskFunc(int (*function)(u32 operation, const u8 *hash));
int setCompareSubTypeFunc(int (*function)(u32 tag));
int setCompareLatestSubTypeFunc(int (*function)(u32 tag));
int prepareModuleHeader(u8 *buffer, SceSize size);
int compareBootModuleLatestSubType(const PspHeader *header);
int getBootModuleLength(u8 *file, SceSize size, u32 unused);
int checkBootModuleHeader(SceLoadCoreExecFileInfo *execInfo);
int decryptBootModule(SceLoadCoreExecFileInfo *execInfo, SceSize *newSize);
SceSize getBootModuleSegmentSpan(const SceLoadCoreExecFileInfo *execInfo,
			       int enabled);
int isBootModuleKl4eCompressed(const SceLoadCoreExecFileInfo *execInfo);
int decompressBootModule(SceLoadCoreExecFileInfo *execInfo);
int checkBootElfSegments(const Elf32_Ehdr *elf,
			 SceLoadCoreExecFileInfo *execInfo);
int checkBootElfSections(const Elf32_Ehdr *elf,
			 SceLoadCoreExecFileInfo *execInfo);
int checkBootElfModule(SceLoadCoreExecFileInfo *execInfo);
void moveBootSegmentWords(const u32 *source, u32 *destination, SceSize size);
int ApplyPspRelSection(const SceLoadCoreExecFileInfo *execInfo,
		      const SceLoadCoreBootSegments *segments,
		      const Elf32_Rel *relocations, int count);
int getRelocationSegmentBits(u32 value);
int isCompressedRelocationHigh16(u32 type);
int applyCompressedBootRelocations(const SceLoadCoreExecFileInfo *execInfo,
				   const SceLoadCoreBootSegments *segments,
				   u32 numSegments, const u8 *data,
				   SceSize size, u32 flags);
int prepareBootSegments(SceLoadCoreExecFileInfo *execInfo,
			SceLoadCoreBootSegments *segments);
void clearBootWords(u32 *destination, u32 count);
void clearBootMemory(u8 *destination, SceSize size);
void copyBootSegments(const SceLoadCoreExecFileInfo *execInfo,
		      const SceLoadCoreBootSegments *segments);

int _loadprx_by_pspbtcnf_bin(SceLoadCoreBootInfo *bootInfo, char *configPath,
			   u32 bootFlags, u32 bootArg);
int setupBootProtection(SceLoadCoreBootInfo *bootInfo);

#endif
