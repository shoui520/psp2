/*
 * Pinned data (source/data.S): everything here lives at the address the
 * original IPL uses.
 */
#ifndef IPL_DATA_H
#define IPL_DATA_H

#include "ipl.h"
#include "loadcore.h"

extern const char g_strLoadprx[], g_strKactivate[], g_strBootMain[], g_strType2[];
extern const char g_strPspbtcnf[], g_strLoadprxByPspbtcnf[], g_strLoaderCore[];
extern const char g_str2RLZ[], g_strKL4E[];
extern const char g_fmtShOverrun[], g_fmtShoff[], g_fmtShName[], g_fmtShType[];
extern const char g_fmtShFlags[], g_fmtShOffset[], g_fmtShSize[], g_fmtShAddralign[];
extern const char g_fmtShEntsize[], g_fmtShOffsetLimit[], g_fmtShNameLimit[];
extern const char g_fmtInternalError[], g_strStars[], g_strCannotLoad[];
extern const char g_fmtBadRelocType[], g_strApplyPspRelSection[];

/* memlmd */
extern const u8 g_unsignXorOut[16], g_unsignXorIn[16];
extern const u8 g_hashHeaderSeed[0x28], g_scrambleXor[16], g_moduleTag[4];
extern u8 *const g_scrambleKey;		/* 0xBFC00220, 16 bytes */
extern u8 *const g_decryptWork;		/* 0xBFC00280, 0xA4 bytes */
extern u8 *const g_decryptHash;		/* 0xBFC00340, 0xB4 bytes */
extern u32 *g_kirkBuffer;
extern u8 *g_whitelist;
extern u8 g_header[0x150];
extern u8 g_keySave[0x28];

/* loadelf */
extern int (*g_getLengthFunc)(u8 *file, SceSize size, SceSize *newSize);
extern int (*g_prepareGetLengthFunc)(u8 *buf, SceSize size);
extern int (*g_setMaskFunc)(u32 unk, const u8 *addr);
extern int (*g_compareSubType)(u32 tag);
extern int (*g_compareLatestSubType)(u32 tag);
extern SceSize g_maxAllocSize;
extern u8 g_relocTypes[0x100];
extern u8 g_relocFlags[0x100];

/* boot */
extern const u32 g_scrambleSeed[8];
extern u32 g_buildVersion;
extern SceSize g_memClearSize;
extern u8 g_iplXorKey[64];
extern const u8 g_iplXorKeyEnd[];
extern u32 g_bootArgs[4];
extern SceLoadCoreBootInfo *g_bootInfo;
extern SceIplHaltEntry g_haltEntry;
extern char g_configPath[0x100];
extern SceLoadCoreExecFileInfo g_execInfo[2];
extern u8 g_bssEnd[];

#endif
