/*
 * slb2.c — IdStorage Leaf Reader, CMAC Verifier & SMI Leaf Authenticator
 *
 * Address range: 0x806064..0x806b77
 *
 * Implements the IdStorage directory scanner and leaf reader over `/ABCDEFGHI`
 * (at byte offset 0x40000 on eMMC), CMAC verification of leaves 0x44 (PSCode)
 * and 0x46..0x47 (OpenPSID), and AES-256-CBC + RSA-2048-PKCS#1v1.5 verification
 * and decryption of SMI leaf 0x80.
 */

#include "second_loader.h"

#define SCE_IDSTORAGE_OFFSET		0x00040000ULL
#define SCE_IDSTORAGE_UNUSED_LEAF	((u16)0xfff5U)

typedef struct SceIdStorageReader SceIdStorageReader;

typedef struct SceIdStorageReaderVtable { /* name ours, C++ vtable for IdStorage reader */
	u32	reserved;
	void	(*dtor)(SceIdStorageReader *self);
	void	(*deleting_dtor)(SceIdStorageReader *self);
} SceIdStorageReaderVtable;

struct SceIdStorageReader { /* name ours, IdStorage directory & leaf reader */
	const SceIdStorageReaderVtable	*vptr;
	int				fd;
	const u8			*mem_base;
	u32				reserved_0c;
	u64				base_offset;
	u32				flags;
	u32				is_valid;
	u32				partition_size;
	u32				dir_sectors;
	u32				max_leaves;
	u32				pad[2];
};

/* .rodata: 0x80c188..0x80c357 */
static const char s_idStorageDevPath0[12] = "/ABCDEFGHI";

static const u8 s_idStorageKeyslot204Seed[16] = {
	0x82, 0x1c, 0x57, 0x14, 0x41, 0x5e, 0x98, 0x04,
	0xd6, 0xaa, 0xe3, 0x24, 0xeb, 0x3d, 0xdd, 0xfe
};

static const u8 s_smiMagic0[8] = { 'S', 'M', 'I', 0x00, 0x00, 0x00, 0x00, 0x00 };
static const u8 s_smiMagic1[8] = { 'S', 'M', 'I', 0x01, 0x00, 0x00, 0x00, 0x00 };

static const char s_idStorageDevPath1[12] = "/ABCDEFGHI";

static const u8 s_smiEncKey0[32] = {
	0xab, 0x70, 0x97, 0x35, 0x6f, 0xdd, 0x49, 0xd8,
	0x38, 0x78, 0x54, 0x01, 0x67, 0xf0, 0xc4, 0xad,
	0x85, 0x53, 0x7c, 0x5a, 0x56, 0xbd, 0x15, 0xdf,
	0x0e, 0xb5, 0xf7, 0xf0, 0xd9, 0xe2, 0x76, 0xe6
};

static const u8 s_smiKey0Iv[16] = {
	0x31, 0x0d, 0x20, 0x07, 0x7a, 0xf3, 0xbf, 0x12,
	0x1f, 0x21, 0xd9, 0xad, 0xaf, 0x38, 0x9c, 0xda
};

static const u8 s_smiBody0Iv[16] = {
	0x2f, 0x0a, 0xee, 0xf9, 0x8e, 0xe3, 0x96, 0x56,
	0x50, 0xf8, 0x48, 0x5e, 0x6c, 0x0b, 0xc2, 0xc4
};

static const u8 s_smiEncKey1[32] = {
	0xd9, 0xa2, 0xca, 0x7f, 0xd2, 0xe5, 0xaa, 0x65,
	0x13, 0x4c, 0xbf, 0x9e, 0xcf, 0x03, 0x6f, 0x8a,
	0xac, 0xe0, 0x2a, 0x42, 0xd9, 0x18, 0x7a, 0x08,
	0x71, 0x7c, 0x02, 0x55, 0xa1, 0x3a, 0xb9, 0x66
};

static const u8 s_smiKey1Iv[16] = {
	0x0f, 0x92, 0xb9, 0x7a, 0xc4, 0x35, 0x33, 0x37,
	0xfd, 0x22, 0x2a, 0x0e, 0x7e, 0x49, 0xeb, 0x58
};

static const u8 s_smiBody1Iv[16] = {
	0x69, 0xd7, 0x61, 0x8b, 0x7d, 0xbe, 0x7b, 0x59,
	0xc0, 0x4d, 0xef, 0x51, 0x69, 0x83, 0x1f, 0xbf
};

static const u8 s_smiRsaModulus[256] = {
	0xc6, 0x18, 0xbe, 0x32, 0x70, 0xe1, 0x55, 0x3b,
	0x2d, 0x08, 0x66, 0x0c, 0x70, 0x00, 0x06, 0x16,
	0x38, 0x38, 0x8d, 0xb3, 0x31, 0xbf, 0xf3, 0x0e,
	0x7d, 0xb6, 0x33, 0x4e, 0xa8, 0xf8, 0xf7, 0xeb,
	0x93, 0x54, 0x0b, 0x66, 0x24, 0x46, 0x5a, 0xbe,
	0x2a, 0xfa, 0xf0, 0x1a, 0xcf, 0x0b, 0x87, 0x05,
	0x1c, 0xba, 0xe1, 0x18, 0x76, 0x91, 0x83, 0xb9,
	0xea, 0x9b, 0x09, 0xc2, 0x25, 0xa0, 0x63, 0xba,
	0x38, 0x7a, 0x23, 0x45, 0x2b, 0xbc, 0xda, 0x29,
	0x49, 0xc4, 0xab, 0xa0, 0xc6, 0x6a, 0x8d, 0x1d,
	0x0a, 0x44, 0xa7, 0xae, 0x22, 0xd1, 0xac, 0x44,
	0x9c, 0x9e, 0xac, 0x5e, 0xd7, 0x9a, 0x18, 0xac,
	0x1b, 0x06, 0x91, 0xb3, 0x45, 0x16, 0x73, 0x96,
	0x5e, 0xd3, 0x36, 0xc9, 0x98, 0x7e, 0xf5, 0xb4,
	0x2f, 0xa6, 0xde, 0x99, 0x5f, 0xfd, 0x31, 0x9a,
	0x62, 0x9a, 0x5e, 0x25, 0x23, 0x16, 0x4e, 0x3f,
	0xb9, 0xc7, 0xbd, 0xe7, 0x02, 0x5e, 0x83, 0x9c,
	0x3b, 0x52, 0x8f, 0x53, 0xe5, 0xab, 0xb0, 0x77,
	0x77, 0x6c, 0x80, 0xe1, 0xc7, 0xa1, 0x57, 0x20,
	0xcb, 0xea, 0x77, 0x0d, 0x76, 0x74, 0xc9, 0xcd,
	0xc4, 0x45, 0x59, 0xf0, 0xed, 0x53, 0x8d, 0xe3,
	0x3f, 0x79, 0xe9, 0x33, 0x4c, 0x62, 0xad, 0x81,
	0x11, 0x4b, 0x23, 0x43, 0x2d, 0x73, 0x6a, 0x89,
	0xce, 0x7a, 0x53, 0xfe, 0xcd, 0x50, 0xe6, 0x6e,
	0x02, 0x1f, 0x9d, 0xcc, 0x2b, 0x63, 0x5e, 0x33,
	0x30, 0x5f, 0xd5, 0xea, 0x8f, 0x15, 0x65, 0x6d,
	0x2b, 0x00, 0xe9, 0x3b, 0xbc, 0x78, 0xee, 0x97,
	0xb6, 0xbd, 0xdd, 0xf8, 0xee, 0x21, 0x82, 0x89,
	0x2d, 0x6a, 0x73, 0x9c, 0xa1, 0x00, 0x1a, 0xd8,
	0x84, 0x9f, 0x2f, 0x0a, 0x25, 0x50, 0x59, 0x2a,
	0x4d, 0xa1, 0x57, 0x42, 0xbb, 0x71, 0x2e, 0x07,
	0xb7, 0x93, 0x3b, 0x8b, 0xfe, 0x1f, 0x2a, 0xd1
};

static const u8 s_smiRsaExponent[8] = {
	0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01
};

static const u32 s_smiHashIndex = 4;

void IdStorageReader_Dtor(SceIdStorageReader *self);
void IdStorageReader_DeletingDtor(SceIdStorageReader *self);
void IdStorageReader_CtorAlt(SceIdStorageReader *self);
void IdStorageReader_Ctor(SceIdStorageReader *self);
void IdStorageReader_DtorBase(SceIdStorageReader *self);
int IdStorageReader_SetGeometry(SceIdStorageReader *self, u32 partSize, u32 dirSectors, u32 maxLeaves);
u32 IdStorageReader_IsValid(const SceIdStorageReader *self);
u32 IdStorageReader_ValidateHeader(SceIdStorageReader *self);
int IdStorageReader_OpenMemory(SceIdStorageReader *self, const void *memBase, u32 flags, u64 baseOffset);
int IdStorageReader_OpenFd(SceIdStorageReader *self, int fd, u32 flags, u64 baseOffset);
int IdStorageReader_FindLeafSlot(SceIdStorageReader *self, u16 leafId, u32 *outSlot);
int IdStorageReader_ReadLeaf(SceIdStorageReader *self, u16 leafId, void *outLeafBuf);

static const SceIdStorageReaderVtable s_idStorageReaderVtable = {
	0,
	IdStorageReader_Dtor,
	IdStorageReader_DeletingDtor
};

/* .bss: 0x80d278 */
static u16 *s_idStorageSectorBuf;

/* 0x806064: readIdStorageLeaf44 (name ours) */
int readIdStorageLeaf44(u8 outSize, void *outBuf, void *sectorBuf, u32 sectorBufSize)
{
	SceIdStorageReader reader;
	u8 sectionBuf[256];
	int fd;
	int res;

	IdStorageReader_Ctor(&reader);
	res = 0x800f0c16;
	if (outSize == 0x10 && outBuf != NULL) {
		res = 0x800f0c1c;
		if (sectorBufSize >= 0x200) {
			fd = blockdevOpen(s_idStorageDevPath0, 0);
			res = 0x800f0c05;
			if (fd != -1) {
				if (IdStorageReader_OpenFd(&reader, fd, 0, SCE_IDSTORAGE_OFFSET) == 0 &&
				    IdStorageReader_IsValid(&reader) != 0) {
					if (IdStorageReader_ReadLeaf(&reader, 0x44, sectorBuf) == 0) {
						blockdevClose(fd);
						memcpy(sectionBuf, (const u8 *)sectorBuf + 0xa0, sizeof(sectionBuf));
						res = idStorageVerifyLeafSection(sectionBuf, sizeof(sectionBuf));
						if (res == 0)
							memcpy(outBuf, sectionBuf, 0x10);
					} else {
						blockdevClose(fd);
					}
				} else {
					blockdevClose(fd);
					res = 0x800f0c26;
				}
			}
		}
	}
	IdStorageReader_Dtor(&reader);
	return res;
}

/* 0x80616e: readIdStorageLeaves46And47 (name ours) */
int readIdStorageLeaves46And47(u8 outSize, void *outBuf, void *sectorBuf, u32 sectorBufSize)
{
	SceIdStorageReader reader;
	u8 sectionBuf[256];
	int fd;
	int res;

	IdStorageReader_Ctor(&reader);
	res = 0x800f0c16;
	if (outSize == 0x10 && outBuf != NULL) {
		res = 0x800f0c1c;
		if (sectorBufSize >= 0x400) {
			fd = blockdevOpen(s_idStorageDevPath0, 0);
			res = 0x800f0c05;
			if (fd != -1) {
				if (IdStorageReader_OpenFd(&reader, fd, 0, SCE_IDSTORAGE_OFFSET) == 0 &&
				    IdStorageReader_IsValid(&reader) != 0) {
					if (IdStorageReader_ReadLeaf(&reader, 0x46, sectorBuf) == 0 &&
					    IdStorageReader_ReadLeaf(&reader, 0x47, (u8 *)sectorBuf + 0x200) == 0) {
						blockdevClose(fd);
						memcpy(sectionBuf, (const u8 *)sectorBuf + 0x128, sizeof(sectionBuf));
						res = idStorageVerifyLeafSection(sectionBuf, sizeof(sectionBuf));
						if (res == 0)
							memcpy(outBuf, sectionBuf, 0x10);
					} else {
						blockdevClose(fd);
					}
				} else {
					blockdevClose(fd);
					res = 0x800f0c26;
				}
			}
		}
	}
	IdStorageReader_Dtor(&reader);
	return res;
}

/* 0x806286: idStorageInitKeyslot204 (name ours) */
u32 idStorageInitKeyslot204(void)
{
	u32 status;

	SCE_BIGMAC_REG(0, SCE_BIGMAC_SRC) = (u32)s_idStorageKeyslot204Seed;
	SCE_BIGMAC_REG(0, SCE_BIGMAC_DST) = 0;
	SCE_BIGMAC_REG(0, SCE_BIGMAC_LEN) = 0x10;
	SCE_BIGMAC_REG(0, SCE_BIGMAC_FUNC) = 0x10000001U;
	SCE_BIGMAC_REG(0, SCE_BIGMAC_KEYSLOT) = 0x204;
	SCE_BIGMAC_REG(0, SCE_BIGMAC_TRIGGER) = 1;
	do {
		status = SCE_BIGMAC_REG(0, SCE_BIGMAC_STATUS);
	} while ((status & 1U) != 0);

	MMIO_W32(0xe0030028U, 0);
	if ((MMIO_R32(0xe003002cU) & 2U) == 0)
		status = 0x800f0c29U;
	return status;
}

/* 0x806308: idStorageComputeLeafCmac (name ours) */
u32 idStorageComputeLeafCmac(void *dst, const void *src, u32 size)
{
	u32 status;

	SCE_BIGMAC_REG(0, SCE_BIGMAC_SRC) = (u32)src;
	SCE_BIGMAC_REG(0, SCE_BIGMAC_DST) = (u32)dst;
	SCE_BIGMAC_REG(0, SCE_BIGMAC_LEN) = size & 0x00ffffffU;
	SCE_BIGMAC_REG(0, SCE_BIGMAC_FUNC) = 0x33bU;
	SCE_BIGMAC_REG(0, SCE_BIGMAC_KEYSLOT) = 0x212;
	SCE_BIGMAC_REG(0, SCE_BIGMAC_TRIGGER) = 1;
	do {
		status = SCE_BIGMAC_REG(0, SCE_BIGMAC_STATUS);
	} while ((status & 1U) != 0);
	return status;
}

/* 0x80634e: idStorageDecryptLeafMac (name ours) */
u32 idStorageDecryptLeafMac(void *dst, const void *src, u32 size)
{
	u32 status;

	if (src == NULL || dst == NULL || size != 0x10)
		return 0x800f0c16U;

	MMIO_W32(0xe0030028U, 0);
	if ((MMIO_R32(0xe003002cU) & 2U) != 2U)
		return 0x800f0c29U;

	SCE_BIGMAC_REG(0, SCE_BIGMAC_SRC) = (u32)src;
	SCE_BIGMAC_REG(0, SCE_BIGMAC_DST) = (u32)dst;
	SCE_BIGMAC_REG(0, SCE_BIGMAC_LEN) = 0x10;
	SCE_BIGMAC_REG(0, SCE_BIGMAC_FUNC) = 0x102U;
	SCE_BIGMAC_REG(0, SCE_BIGMAC_KEYSLOT) = 0;
	SCE_BIGMAC_REG(0, SCE_BIGMAC_TRIGGER) = 1;
	do {
		status = SCE_BIGMAC_REG(0, SCE_BIGMAC_STATUS);
	} while ((status & 1U) != 0);
	return status;
}

/* 0x8063c2: idStorageVerifyLeafSection (name ours) */
int idStorageVerifyLeafSection(const void *section, u32 size)
{
	u8 computedCmac[16];
	u8 expectedCmac[16];
	int res;

	if (size != 0x100)
		return 0x800f0c16;

	memset(computedCmac, 0, sizeof(computedCmac));
	memset(expectedCmac, 0, sizeof(expectedCmac));

	res = (int)idStorageInitKeyslot204();
	if (res == 0) {
		res = (int)idStorageComputeLeafCmac(computedCmac, section, 0xd8);
		if (res == 0) {
			res = (int)idStorageDecryptLeafMac(expectedCmac, (const u8 *)section + 0xd8, 0x10);
			if (res == 0 && memcmp(computedCmac, expectedCmac, 0x10) != 0)
				res = 0x800f0c27;
		}
	}
	return res;
}

/* 0x806440: idStorageExtractKeywordFlag (name ours) */
u32 idStorageExtractKeywordFlag(u32 val)
{
	return zeroExtendByte((u8)(bswap32(val) >> 26));
}

/* 0x806458: idStorageDecryptKeyWithSlot213 (name ours) */
int idStorageDecryptKeyWithSlot213(void *dstKey, const void *srcKey, const void *iv)
{
	SceBigmacContext ctx;
	u32 keyslot;
	u8 ivBuf[16];
	int res;

	bigmacInitContext(&ctx);
	bigmacSetChannel(&ctx, 0);
	keyslot = 0x213;
	memcpy(ivBuf, iv, sizeof(ivBuf));
	res = bigmacAes256CbcDecryptKeyslot(dstKey, srcKey, 0x20, &keyslot, ivBuf, &ctx);
	if (res == 0)
		res = bigmacWait(&ctx);
	return res;
}

/* 0x8064cc: idStorageAes256CbcDecrypt (name ours) */
int idStorageAes256CbcDecrypt(void *dst, const void *src, u32 size, const void *key, const void *iv)
{
	SceBigmacContext ctx;
	u8 ivBuf[16];
	int res;

	bigmacInitContext(&ctx);
	memcpy(ivBuf, iv, sizeof(ivBuf));
	res = bigmacAes256CbcDecrypt(dst, src, size, key, ivBuf, &ctx);
	if (res == 0)
		res = bigmacWait(&ctx);
	return res;
}

/* 0x80653a: idStorageVerifyAndDecryptSmiLeaf (name ours) */
int idStorageVerifyAndDecryptSmiLeaf(u8 *leaf)
{
	u8 key0[32];
	u8 key1[32];
	SceRsaPublicKey rsaKey;
	SceBigmacContext shaCtx;
	u32 bignumStep;
	u8 rsaOut[256];
	u8 sha256Digest[32];
	u32 i;
	int res;

	if (memcmp(leaf, s_smiMagic0, 4) != 0 && memcmp(leaf, s_smiMagic1, 4) != 0)
		return SCE_SBL_ERROR_SL_EILLEGAL;

	if (*(const u16 *)(leaf + 4) > 1 || *(const u16 *)(leaf + 6) != 0)
		return SCE_SBL_ERROR_SL_EILLEGAL;

	for (i = 0; i < 0x74; i++) {
		if (leaf[0x0c + i] != 0)
			return SCE_SBL_ERROR_SL_EILLEGAL;
	}

	memset(key0, 0, sizeof(key0));
	res = idStorageDecryptKeyWithSlot213(key0, s_smiEncKey0, s_smiKey0Iv);
	if (res != 0)
		return res;

	res = idStorageAes256CbcDecrypt(leaf + 0x80, leaf + 0x80, 0x180, key0, s_smiBody0Iv);
	if (res != 0)
		return res;

	bigmacInitContext(&shaCtx);
	memset(sha256Digest, 0, sizeof(sha256Digest));
	res = selfBigmacSha256Start(sha256Digest, leaf, 0x100, &shaCtx);
	if (res != 0)
		return res;
	res = bigmacWait(&shaCtx);
	if (res != 0)
		return res;

	memset(rsaOut, 0, sizeof(rsaOut));
	bignumInitState(&bignumStep);
	rsaKey.modulus = (const u32 *)s_smiRsaModulus;
	rsaKey.modulus_words = 64;
	rsaKey.exponent = (const u32 *)s_smiRsaExponent;
	rsaKey.exponent_words = 1;
	for (;;) {
		res = verifyRsaPkcs1v15Step(rsaOut, leaf + 0x100, &rsaKey, sha256Digest, s_smiHashIndex, &bignumStep);
		if ((u32)res != 0x800f0010U)
			break;
		bignumWaitInterrupt();
	}
	if (res != 0)
		return res;

	memset(key1, 0, sizeof(key1));
	res = idStorageDecryptKeyWithSlot213(key1, s_smiEncKey1, s_smiKey1Iv);
	if (res != 0)
		return res;

	return idStorageAes256CbcDecrypt(leaf + 0x80, leaf + 0x80, 0x80, key1, s_smiBody1Iv);
}

/* 0x8066c8: readIdStorageSmiLeaf80 (name ours) */
int readIdStorageSmiLeaf80(void *outBuf, u32 outSize, void *sectorBuf, u32 sectorBufSize)
{
	SceIdStorageReader reader;
	int fd;
	int res;

	IdStorageReader_Ctor(&reader);
	res = SCE_SBL_ERROR_SL_EINVAL;
	if (outSize == 0x200 && outBuf != NULL) {
		res = 0x800f001c;
		if (sectorBufSize >= 0x200) {
			fd = blockdevOpen(s_idStorageDevPath1, 0);
			res = SCE_SBL_ERROR_SL_EIO;
			if (fd != -1) {
				res = IdStorageReader_OpenFd(&reader, fd, 0, SCE_IDSTORAGE_OFFSET);
				if (res == 0) {
					if (IdStorageReader_IsValid(&reader) == 0) {
						blockdevClose(fd);
						res = 0x800f0026;
					} else {
						res = IdStorageReader_ReadLeaf(&reader, 0x80, sectorBuf);
						blockdevClose(fd);
						if (res == 0) {
							bigmacMemcpy(outBuf, sectorBuf, 0x200);
							res = idStorageVerifyAndDecryptSmiLeaf((u8 *)outBuf);
						}
					}
				} else {
					blockdevClose(fd);
				}
			}
		}
	}
	IdStorageReader_Dtor(&reader);
	return res;
}

/* 0x80679c: IdStorageReader_Dtor (name ours) */
void IdStorageReader_Dtor(SceIdStorageReader *self)
{
	self->vptr = &s_idStorageReaderVtable;
}

/* 0x8067a4: IdStorageReader_DeletingDtor (name ours) */
void IdStorageReader_DeletingDtor(SceIdStorageReader *self)
{
	self->vptr = &s_idStorageReaderVtable;
	operatorDeleteNoop(self);
}

/* 0x8067ae: IdStorageReader_CtorAlt (name ours) */
void IdStorageReader_CtorAlt(SceIdStorageReader *self)
{
	self->vptr = &s_idStorageReaderVtable;
	self->fd = -1;
	self->mem_base = NULL;
	self->flags = 0;
	self->is_valid = 0;
	self->partition_size = 0x80000;
	self->dir_sectors = 0x20;
	self->base_offset = 0;
	self->max_leaves = 0x400;
}

/* 0x8067ec: IdStorageReader_Ctor (name ours) */
void IdStorageReader_Ctor(SceIdStorageReader *self)
{
	self->vptr = &s_idStorageReaderVtable;
	self->fd = -1;
	self->mem_base = NULL;
	self->flags = 0;
	self->is_valid = 0;
	self->partition_size = 0x80000;
	self->dir_sectors = 0x20;
	self->base_offset = 0;
	self->max_leaves = 0x400;
}

/* 0x80682a: IdStorageReader_DtorBase (name ours) */
void IdStorageReader_DtorBase(SceIdStorageReader *self)
{
	self->vptr = &s_idStorageReaderVtable;
}

/* 0x806832: IdStorageReader_SetGeometry (name ours) */
int IdStorageReader_SetGeometry(SceIdStorageReader *self, u32 partSize, u32 dirSectors, u32 maxLeaves)
{
	self->partition_size = partSize;
	self->dir_sectors = dirSectors;
	self->max_leaves = maxLeaves;
	return 0;
}

/* 0x806842: IdStorageReader_IsValid (name ours) */
u32 IdStorageReader_IsValid(const SceIdStorageReader *self)
{
	return self->is_valid;
}

/* 0x806848: IdStorageReader_ValidateHeader (name ours) */
u32 IdStorageReader_ValidateHeader(SceIdStorageReader *self)
{
	u32 i;

	if (self->mem_base == NULL && self->fd == -1)
		return SCE_SBL_ERROR_SL_EINVAL;
	if (s_idStorageSectorBuf == NULL)
		return 0x800f0026U;

	memset(s_idStorageSectorBuf, 0, 0x200);
	if (self->fd == -1) {
		memcpy(s_idStorageSectorBuf, self->mem_base + (u32)self->base_offset, 0x200);
	} else {
		if (blockdevLseek(self->fd, (s64)self->base_offset, 0) == -1LL)
			return 0;
		if (blockdevRead(self->fd, s_idStorageSectorBuf, 0x200) != 0x200)
			return 0;
	}

	for (i = 0; i < self->dir_sectors; i++) {
		if (s_idStorageSectorBuf[i] != SCE_IDSTORAGE_UNUSED_LEAF)
			return 0;
	}

	self->is_valid = 1;
	return 1;
}

/* 0x806916: IdStorageReader_OpenMemory (name ours) */
int IdStorageReader_OpenMemory(SceIdStorageReader *self, const void *memBase, u32 flags, u64 baseOffset)
{
	if (memBase == NULL)
		return SCE_SBL_ERROR_SL_EINVAL;
	if (s_idStorageSectorBuf == NULL)
		return 0x800f0026;

	self->fd = -1;
	self->mem_base = (const u8 *)memBase;
	self->flags = flags;
	self->base_offset = baseOffset;
	self->is_valid = IdStorageReader_ValidateHeader(self);
	return 0;
}

/* 0x806964: IdStorageReader_OpenFd (name ours) */
int IdStorageReader_OpenFd(SceIdStorageReader *self, int fd, u32 flags, u64 baseOffset)
{
	if (fd <= 0)
		return SCE_SBL_ERROR_SL_EINVAL;
	if (s_idStorageSectorBuf == NULL)
		return 0x800f0026;

	self->fd = fd;
	self->mem_base = NULL;
	self->flags = flags;
	self->base_offset = baseOffset;
	self->is_valid = IdStorageReader_ValidateHeader(self);
	return 0;
}

/* 0x8069b4: IdStorageReader_FindLeafSlot (name ours) */
int IdStorageReader_FindLeafSlot(SceIdStorageReader *self, u16 leafId, u32 *outSlot)
{
	u32 sectorIdx;
	u32 slotIdx;
	u32 i;

	if (self->is_valid == 0)
		return SCE_SBL_ERROR_SL_EILLEGAL;

	if (self->fd != -1) {
		if (blockdevLseek(self->fd, (s64)self->base_offset, 0) == -1LL)
			return SCE_SBL_ERROR_SL_EIO;
	}

	if (s_idStorageSectorBuf == NULL)
		return 0x800f0026;

	slotIdx = 0;
	for (sectorIdx = 0; sectorIdx < self->dir_sectors; sectorIdx++) {
		memset(s_idStorageSectorBuf, 0, 0x200);
		if (self->fd == -1) {
			memcpy(s_idStorageSectorBuf,
			       self->mem_base + sectorIdx * 0x200U + (u32)self->base_offset,
			       0x200);
		} else {
			if (blockdevRead(self->fd, s_idStorageSectorBuf, 0x200) != 0x200)
				return SCE_SBL_ERROR_SL_EIO;
		}

		for (i = 0; i < 0x100; i++, slotIdx++) {
			if (s_idStorageSectorBuf[i] == leafId) {
				*outSlot = slotIdx;
				return 0;
			}
		}
	}

	return SCE_SBL_ERROR_SL_ENOENT;
}

/* 0x806ab0: IdStorageReader_ReadLeaf (name ours) */
int IdStorageReader_ReadLeaf(SceIdStorageReader *self, u16 leafId, void *outLeafBuf)
{
	u32 slotIdx;
	int res;

	if (self->is_valid == 0)
		return SCE_SBL_ERROR_SL_EILLEGAL;

	res = IdStorageReader_FindLeafSlot(self, leafId, &slotIdx);
	if (res != 0)
		return res;

	if (self->fd == -1) {
		memcpy(outLeafBuf,
		       self->mem_base + slotIdx * 0x200U + (u32)self->base_offset,
		       0x200);
	} else {
		s64 leafOffset = (s64)(self->base_offset + (u64)(slotIdx * 0x200U));

		if (blockdevLseek(self->fd, leafOffset, 0) == -1LL ||
		    blockdevRead(self->fd, outLeafBuf, 0x200) != 0x200)
			res = SCE_SBL_ERROR_SL_EIO;
	}

	return res;
}

/* 0x806b58: idStorageSetSectorBuf (name ours) */
int idStorageSetSectorBuf(void *buf, u32 size)
{
	if (buf == NULL)
		return SCE_SBL_ERROR_SL_EINVAL;
	if (size < 0x200)
		return 0x800f001c;
	s_idStorageSectorBuf = (u16 *)buf;
	return 0;
}
