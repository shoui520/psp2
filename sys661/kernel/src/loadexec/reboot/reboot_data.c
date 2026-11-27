/*
 * Pure C data, strings, memlmd key constants, and BSS buffers for ePSP reboot.bin.
 */

#include "ipl_data.h"

const char g_strLoadprx[] = "loadprx";
const char g_strKactivate[] = "kactivate";
const char g_strBootMain[] = "boot_main";
const char g_strType2[] = "type2";
const char g_strPspbtcnf[] = "pspbtcnf.bin";
const char g_strLoadprxByPspbtcnf[] = "_loadprx_by_pspbtcnf_bin";
const char g_strLoaderCore[] = "sceLoaderCore";
const char g_str2RLZ[] = "2RLZ";
const char g_strKL4E[] = "KL4E";
const char g_fmtShOverrun[] = "%4d: 0x%08x < sh_offset+sh_size 0x%08x\n";
const char g_fmtShoff[] = "ehdr->e_shoff     0x%08x\n";
const char g_fmtShName[] = "%4d/%4d: sh_name      0x%08x\n";
const char g_fmtShType[] = "%4d: sh_type      0x%08x\n";
const char g_fmtShFlags[] = "%4d: sh_flags     0x%08x\n";
const char g_fmtShOffset[] = "%4d: sh_offset    0x%08x\n";
const char g_fmtShSize[] = "%4d: sh_size      0x%08x\n";
const char g_fmtShAddralign[] = "%4d: sh_addralign 0x%08x\n";
const char g_fmtShEntsize[] = "%4d: sh_entsize   0x%08x\n";
const char g_fmtShOffsetLimit[] = "%4d: 0x%08x < sh_offset 0x%08x\n";
const char g_fmtShNameLimit[] = "%4d/%4d: max 0x%08x < sh_name 0x%08x\n";
const char g_fmtInternalError[] = "../../sys661/kernel/src/loadcore/loadelf_boot.c:%s:internal error !\n";
const char g_strStars[] = "********************\n";
const char g_strCannotLoad[] = "PSP cannot load this image\n";
const char g_fmtBadRelocType[] = "unacceptable relocation type: 0x%x\n";
const char g_strApplyPspRelSection[] = "ApplyPspRelSection";

/* memlmd header unsign masks, ECDSA hash seed, scramble XOR mask, and 3.65 kernel tag */
const u8 g_unsignXorOut[16] = {
	0x71, 0xF6, 0xA8, 0x31, 0x1E, 0xE0, 0xFF, 0x1E,
	0x50, 0xBA, 0x6C, 0xD2, 0x98, 0x2D, 0xD6, 0x2D,
};

const u8 g_unsignXorIn[16] = {
	0xAA, 0x85, 0x4D, 0xB0, 0xFF, 0xCA, 0x47, 0xEB,
	0x38, 0x7F, 0xD7, 0xE4, 0x3D, 0x62, 0xB0, 0x10,
};

const u8 g_hashHeaderSeed[0x28] = {
	0x77, 0x3F, 0x4B, 0xE1, 0x4C, 0x0A, 0xB4, 0x52,
	0x67, 0x2B, 0x67, 0x56, 0x82, 0x4C, 0xCF, 0x42,
	0xAA, 0x37, 0xFF, 0xC0, 0x89, 0x41, 0xE5, 0x63,
	0x5E, 0x84, 0xE9, 0xFB, 0x53, 0xDA, 0x94, 0x9E,
	0x9B, 0xB7, 0xC2, 0xA4, 0x22, 0x9F, 0xDF, 0x1F,
};

const u8 g_scrambleXor[16] = {
	0x09, 0xEE, 0x7E, 0x6D, 0x7A, 0xE9, 0xA6, 0xDE,
	0x79, 0xC0, 0xBA, 0x24, 0xA3, 0x88, 0x1D, 0x14,
};

const u8 g_moduleTag[4] = { 0xF0, 0xA1, 0x94, 0x4C };

/* Scratchpad buffers used by the memlmd decrypter */
u8 *const g_scrambleKey = (u8 *)0xBFC00220;
u8 *const g_decryptWork = (u8 *)0xBFC00280;
u8 *const g_decryptHash = (u8 *)0xBFC00340;

/* loadelf hook callbacks */
int (*g_getLengthFunc)(u8 *file, SceSize size, SceSize *newSize);
int (*g_prepareGetLengthFunc)(u8 *buf, SceSize size);
int (*g_setMaskFunc)(u32 mode, const u8 *addr);
int (*g_compareSubType)(u32 tag);
int (*g_compareLatestSubType)(u32 tag);
SceSize g_maxAllocSize;

/* BSS state cleared by reboot main() */
u32 *g_kirkBuffer;
u8 *g_whitelist;
u8 __attribute__((aligned(64))) g_header[0x150];
u8 __attribute__((aligned(64))) g_keySave[0x28];
u8 g_relocTypes[0x100];
u8 g_relocFlags[0x100];
SceLoadCoreBootInfo *g_bootInfo;
SceIplHaltEntry g_haltEntry;
char g_configPath[0x100];
SceLoadCoreExecFileInfo g_execInfo[2];
