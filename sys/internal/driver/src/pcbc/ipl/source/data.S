/*
 * Data at the addresses the original IPL uses: strings, key material and
 * boot state. The Vita side and the scratchpad layout (0xBFC00200-0xBFC00FFF)
 * depend on some of it; keeping all of it pinned also lets the rebuild be
 * compared with the original byte for byte in memory.
 */

#define OBJECT(name, addr)		\
	.org	(addr) - DATA_BASE;	\
	.globl	name;			\
	.type	name, @object;		\
name:

#define STRING(name, addr, text)	\
	OBJECT(name, addr)		\
	.asciz	text;			\
	.size	name, . - name

#define DATA_BASE	0x8860508C

	.section .data.pinned, "aw"

STRING(g_strLoadprx,		0x8860508C, "loadprx")
STRING(g_strKactivate,		0x88605094, "kactivate")
STRING(g_strBootMain,		0x886050A0, "boot_main")
STRING(g_strType2,		0x886050AC, "type2")
STRING(g_strPspbtcnf,		0x886050B4, "pspbtcnf.bin")
STRING(g_strLoadprxByPspbtcnf,	0x886050C4, "_loadprx_by_pspbtcnf_bin")
STRING(g_strLoaderCore,		0x886050E0, "sceLoaderCore")
STRING(g_str2RLZ,		0x886050F0, "2RLZ")
STRING(g_strKL4E,		0x886050F8, "KL4E")
STRING(g_fmtShOverrun,		0x88605100, "%4d: 0x%08x < sh_offset+sh_size 0x%08x\n")
STRING(g_fmtShoff,		0x88605128, "ehdr->e_shoff     0x%08x\n")
STRING(g_fmtShName,		0x88605144, "%4d/%4d: sh_name      0x%08x\n")
STRING(g_fmtShType,		0x88605164, "%4d: sh_type      0x%08x\n")
STRING(g_fmtShFlags,		0x88605180, "%4d: sh_flags     0x%08x\n")
STRING(g_fmtShOffset,		0x8860519C, "%4d: sh_offset    0x%08x\n")
STRING(g_fmtShSize,		0x886051B8, "%4d: sh_size      0x%08x\n")
STRING(g_fmtShAddralign,	0x886051D4, "%4d: sh_addralign 0x%08x\n")
STRING(g_fmtShEntsize,		0x886051F0, "%4d: sh_entsize   0x%08x\n")
STRING(g_fmtShOffsetLimit,	0x8860520C, "%4d: 0x%08x < sh_offset 0x%08x\n")
STRING(g_fmtShNameLimit,	0x8860522C, "%4d/%4d: max 0x%08x < sh_name 0x%08x\n")
STRING(g_fmtInternalError,	0x88605254, "../../sys661/kernel/src/loadcore/loadelf_boot.c:%s:internal error !\n")
STRING(g_strStars,		0x8860529C, "********************\n")
STRING(g_strCannotLoad,		0x886052B4, "PSP cannot load this image\n")
STRING(g_fmtBadRelocType,	0x886052D0, "unacceptable relocation type: 0x%x\n")
STRING(g_strApplyPspRelSection,	0x886052F4, "ApplyPspRelSection")

/* memlmd: header unsign masks, hash seed, scramble key mask, module tag */
OBJECT(g_unsignXorOut,		0x88605308)
	.byte	0x71, 0xF6, 0xA8, 0x31, 0x1E, 0xE0, 0xFF, 0x1E
	.byte	0x50, 0xBA, 0x6C, 0xD2, 0x98, 0x2D, 0xD6, 0x2D
OBJECT(g_unsignXorIn,		0x88605318)
	.byte	0xAA, 0x85, 0x4D, 0xB0, 0xFF, 0xCA, 0x47, 0xEB
	.byte	0x38, 0x7F, 0xD7, 0xE4, 0x3D, 0x62, 0xB0, 0x10
OBJECT(g_hashHeaderSeed,	0x88605328)
	.byte	0x77, 0x3F, 0x4B, 0xE1, 0x4C, 0x0A, 0xB4, 0x52
	.byte	0x67, 0x2B, 0x67, 0x56, 0x82, 0x4C, 0xCF, 0x42
	.byte	0xAA, 0x37, 0xFF, 0xC0, 0x89, 0x41, 0xE5, 0x63
	.byte	0x5E, 0x84, 0xE9, 0xFB, 0x53, 0xDA, 0x94, 0x9E
	.byte	0x9B, 0xB7, 0xC2, 0xA4, 0x22, 0x9F, 0xDF, 0x1F
OBJECT(g_scrambleXor,		0x88605350)
	.byte	0xCA, 0x20, 0xCD, 0xCC, 0xEF, 0xB3, 0x4C, 0x41
	.byte	0xB1, 0xBF, 0x20, 0xC3, 0xCA, 0x8D, 0xE4, 0x15
OBJECT(g_moduleTag,		0x88605360)
	.word	0x4C94A0F0		/* kernel modules, 3.57-3.73 key */

/* boot: seed of the scramble key, placed at 0xBFC00200 */
OBJECT(g_scrambleSeed,		0x88605368)
#include IPL_SCRAMBLE_SEED_FILE
	.space	16

/* 0x88605388-0x886053CB: the original's switch tables (compiler data) */

/* loadelf hooks (loadcore's sceKernelSet*Function) */
OBJECT(g_getLengthFunc,		0x886053CC)
	.word	0
OBJECT(g_prepareGetLengthFunc,	0x886053D0)
	.word	0
OBJECT(g_setMaskFunc,		0x886053D4)
	.word	0
OBJECT(g_compareSubType,	0x886053D8)
	.word	0
OBJECT(g_compareLatestSubType,	0x886053DC)
	.word	0
OBJECT(g_maxAllocSize,		0x886053E0)
	.word	0

/* scratchpad buffers of the decrypter */
OBJECT(g_scrambleKey,		0x886053E4)
	.word	0xBFC00220
OBJECT(g_decryptWork,		0x886053E8)
	.word	0xBFC00280
OBJECT(g_decryptHash,		0x886053EC)
	.word	0xBFC00340

/* defaults pspbtcnf.bin mode entries may override */
OBJECT(g_buildVersion,		0x886053F0)
	.word	0x06060010		/* 6.60 */
OBJECT(g_memClearSize,		0x886053F4)
	.word	0x00300000

/* External 3.60+ IPL header-XOR key handed to the next stage. */
OBJECT(g_iplXorKey,		0x886053F8)
	.byte	0x59, 0xAD, 0x29, 0xD3, 0xE6, 0x62, 0x79, 0xF1
	.byte	0xAF, 0x53, 0x2C, 0x62, 0x79, 0x92, 0xDE, 0xCB
	.byte	0x56, 0xA8, 0xB9, 0x9C, 0x68, 0xA5, 0x09, 0x58
	.byte	0x18, 0xF3, 0x52, 0xDC, 0x9B, 0xC7, 0xFB, 0x8F
	.byte	0x3D, 0x43, 0x70, 0x7D, 0x2F, 0xBB, 0x72, 0x3C
	.byte	0x12, 0x36, 0x0C, 0x8E, 0x81, 0xBE, 0x03, 0x1E
	.byte	0x01, 0x2F, 0x20, 0xD2, 0x68, 0xDA, 0x7C, 0xCD
	.byte	0x20, 0x21, 0xD5, 0x56, 0x0F, 0xE6, 0x27, 0x02
	.globl	g_iplXorKeyEnd
g_iplXorKeyEnd:

#undef DATA_BASE
#define DATA_BASE	0x88605440

	.section .bss.pinned, "aw", @nobits

OBJECT(g_kirkBuffer,		0x88605440)
	.space	4
OBJECT(g_whitelist,		0x88605444)
	.space	4
OBJECT(g_header,		0x88605480)	/* saved "~PSP" header */
	.space	0x150
OBJECT(g_keySave,		0x88605600)
	.space	0x28
OBJECT(g_relocTypes,		0x88605640)	/* compressed relocation lookup tables */
	.space	0x100
OBJECT(g_relocFlags,		0x88605740)
	.space	0x100
OBJECT(g_bootArgs,		0x88605840)
	.space	16
OBJECT(g_bootInfo,		0x88605850)
	.space	4
OBJECT(g_haltEntry,		0x88605854)
	.space	4
OBJECT(g_configPath,		0x88605858)
	.space	0x100
OBJECT(g_execInfo,		0x88605958)	/* sceLoaderCore, sysmem */
	.space	2 * 0xC0
	.globl	g_bssEnd
g_bssEnd:					/* 0x88605AD8 */
