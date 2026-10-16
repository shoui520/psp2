/*
 * "~PSP" executable header (0x150 bytes) of an encrypted PSP module.
 */
#ifndef PSP_HEADER_H
#define PSP_HEADER_H

#include "ipl.h"

#define PSP_MAGIC		0x5053507E	/* "~PSP", read little-endian */
#define SCE_MAGIC		0x4543537E	/* "~SCE" */
#define PSP_HEADER_SIZE		0x150

typedef struct PspHeader {
	u32	magic;			/* 0x00 */
	u16	modAttribute;		/* 0x04 */
	u16	compAttribute;		/* 0x06 */
	u8	moduleVerLo;		/* 0x08 */
	u8	moduleVerHi;		/* 0x09 */
	char	modName[28];		/* 0x0A */
	u8	modVersion;		/* 0x26 */
	u8	nSegments;		/* 0x27 */
	u32	elfSize;		/* 0x28 */
	u32	pspSize;		/* 0x2C */
	u32	bootEntry;		/* 0x30 */
	u32	modInfoOffset;		/* 0x34 */
	u32	bssSize;		/* 0x38 */
	u16	segAlign[4];		/* 0x3C */
	u32	segAddress[4];		/* 0x44 */
	u32	segSize[4];		/* 0x54 */
	u32	reserved[5];		/* 0x64 */
	u32	devkitVersion;		/* 0x78 */
	u8	decryptMode;		/* 0x7C */
	u8	padding;		/* 0x7D */
	u16	overlapSize;		/* 0x7E */
	u8	aesKey[0x10];		/* 0x80 */
	u8	cmacKey[0x10];		/* 0x90 */
	u8	cmacHeaderHash[0x10];	/* 0xA0 */
	u32	compSize;		/* 0xB0 */
	u32	reserved1;		/* 0xB4 */
	u32	reserved2[2];		/* 0xB8 */
	u8	cmacDataHash[0x10];	/* 0xC0 */
	u32	tag;			/* 0xD0 */
	u8	sigCheck[0x58];		/* 0xD4 */
	u8	sha1Hash[0x14];		/* 0x12C */
	u8	keyData4[0x10];		/* 0x140 */
} PspHeader;

/* Header plus the first data block copied by the IPL's preparation path. */
typedef struct PspHeaderSnapshot {
	PspHeader header;
	u8 firstDataBlock[16];
} PspHeaderSnapshot;

_Static_assert(sizeof(PspHeader) == 0x150, "PSP executable header");
_Static_assert(sizeof(PspHeaderSnapshot) == 0x160, "IPL header snapshot");

#endif
