/*
 * The sector ciphers' wrappers: one call per sector run, the sector's number
 * counting up as a 128-bit little-endian value. flags bit 0 runs the cipher
 * into the scratch buffer and copies the source unchanged; the software pair
 * takes the CBC IV (or the CTR counter) from the sector's number xored with
 * the tweak key.
 *
 * Names: the ones Sony's records give (3.200.010-CEXTOOL), and ours where no
 * record names the function.
 */

#include <sysmem/sysclib.h>
#include <sysmem/aes.h>
#include <sysmem/utils.h>
#include <ss_mgr/ss_mgr.h>
#include "pfsmgr_internal.h"

/* where a sector cipher that only reports its result puts it (name and size
 * unknown) */
static SceUInt8 g_pfsSectorScratch[0x800];

/* the tweak step, in crypt/xts_dmac5.c (names unknown) */
void pfsSectorTweakEncrypt(const SceUInt32 *src, const SceUInt32 *tweak, SceUInt32 *dst,
			   SceSize size);
void pfsSectorTweakDecrypt(const SceUInt32 *src, const SceUInt32 *tweak, SceUInt32 *dst,
			   SceSize size);

/* the software pair: the tweak the sector's number encrypts to is the CBC IV,
 * and the data goes through the engine with the tweak key's sector mixed in */
static int pfsSectorCryptCbcSwEncrypt(const void *tweak, const void *key, const void *tweakKey,
				   SceUInt32 keyBits, SceSize size, const void *src, void *dst)
{
	SceAesCompactContext ctx;
	SceUInt32 block[4];
	SceUInt32 iv[4];
	int ret;

	SceKernelUtilsForDriver_EDA97D6D(&ctx, 128, keyBits, tweakKey);
	SceKernelUtilsForDriver_302947B6(&ctx, tweak, block);
	pfsSectorTweakDecrypt(src, block, dst, size);
	ret = SceSblSsMgrForDriver_1B14658D(src, dst, size, key, keyBits, iv, 1, 0);
	if (ret == 0)
		pfsSectorTweakDecrypt(dst, block, dst, size);
	return ret;
}

static int pfsSectorCryptCbcSwDecrypt(const void *tweak, const void *key, const void *tweakKey,
				   SceUInt32 keyBits, SceSize size, const void *src, void *dst)
{
	SceAesCompactContext ctx;
	SceUInt32 block[4];
	SceUInt32 iv[4];
	int ret;

	SceKernelUtilsForDriver_EDA97D6D(&ctx, 128, keyBits, tweakKey);
	SceKernelUtilsForDriver_302947B6(&ctx, tweak, block);
	pfsSectorTweakDecrypt(src, block, dst, size);
	ret = SceSblSsMgrForDriver_1B14658D(src, dst, size, key, keyBits, iv, 1, 0);
	if (ret == 0)
		pfsSectorTweakDecrypt(dst, block, dst, size);
	return ret;
}

/* every block of a sector through the engine's ciphers, the sector's 128-bit
 * number counting up from block to block: the engine path takes the number's
 * AES as the tweak, the software path its xor with the tweak key as the IV */
int XTS_Encrypt_ext(const void *key, const void *tweakKey, SceUInt32 keyBits, SceUInt64 sector,
		    SceSize size, SceUInt32 sectorSize, const void *src, void *dst, SceUInt16 flags)
{
	SceUInt8 block[0x10];
	SceUInt8 *tweak, *p;
	SceSize remaining, offset;
	SceUInt32 (*crypt)(const void *, const void *, const void *, SceUInt32, SceSize,
			   const void *, void *) = XTS_EncryptSector_dmac5;
	int ret;

	if (((sectorSize | size) & 0xf) != 0 || size <= 0xf)
		return PFS_ERROR_SECTOR_SIZE;
	if ((((SceUInt32)(SceUIntPtr)src | (SceUInt32)(SceUIntPtr)dst) & 3) != 0)
		return PFS_ERROR_SECTOR_ALIGN;
	tweak = block;
	do {
		*tweak++ = (SceUInt8)sector;
		sector >>= 8;
	} while (tweak != block + 8);
	do {
		*tweak++ = 0;
	} while (tweak != block + 0x10);
	remaining = size;
	offset = 0;
	do {
		SceSize len = sectorSize < remaining ? sectorSize : remaining;
		void *out = (SceUInt8 *)dst + offset;

		if ((flags & 1) != 0) {
			crypt = pfsSectorCryptCbcSwEncrypt;
			out = g_pfsSectorScratch;
		}
		ret = crypt(block, key, tweakKey, keyBits, len, (const SceUInt8 *)src + offset, out);
		if (ret != 0)
			return ret;
		/* the sector's number counts up for the next one */
		p = block;
		do {
			SceUInt8 digit = *p;

			if (digit != 0xff) {
				*p = digit + 1;
				break;
			}
			*p++ = 0;
		} while (p != block + 0x10);
		remaining -= sectorSize;
		offset += sectorSize;
	} while (size > offset);
	if ((flags & 1) != 0 && dst != src)
		memcpy(dst, src, size);
	return 0;
}

int XTS_Decrypt_ext(const void *key, const void *tweakKey, SceUInt32 keyBits, SceUInt64 sector,
		    SceSize size, SceUInt32 sectorSize, const void *src, void *dst, SceUInt16 flags)
{
	SceUInt8 block[0x10];
	SceUInt8 *tweak, *p;
	SceSize remaining, offset;
	SceUInt32 (*crypt)(const void *, const void *, const void *, SceUInt32, SceSize,
			   const void *, void *) = XTS_DecryptSector_dmac5;
	int ret;

	if (((sectorSize | size) & 0xf) != 0 || size <= 0xf)
		return PFS_ERROR_SECTOR_SIZE;
	if ((((SceUInt32)(SceUIntPtr)src | (SceUInt32)(SceUIntPtr)dst) & 3) != 0)
		return PFS_ERROR_SECTOR_ALIGN;
	tweak = block;
	do {
		*tweak++ = (SceUInt8)sector;
		sector >>= 8;
	} while (tweak != block + 8);
	do {
		*tweak++ = 0;
	} while (tweak != block + 0x10);
	remaining = size;
	offset = 0;
	do {
		SceSize len = sectorSize < remaining ? sectorSize : remaining;
		void *out = (SceUInt8 *)dst + offset;

		if ((flags & 1) != 0) {
			crypt = pfsSectorCryptCbcSwDecrypt;
			out = g_pfsSectorScratch;
		}
		ret = crypt(block, key, tweakKey, keyBits, len, (const SceUInt8 *)src + offset, out);
		if (ret != 0)
			return ret;
		p = block;
		do {
			SceUInt8 digit = *p;

			if (digit != 0xff) {
				*p = digit + 1;
				break;
			}
			*p++ = 0;
		} while (p != block + 0x10);
		remaining -= sectorSize;
		offset += sectorSize;
	} while (size > offset);
	if ((flags & 1) != 0 && dst != src)
		memcpy(dst, src, size);
	return 0;
}

/* the software path: the CBC or CTR cipher runs on each of the sector's
 * blocks with the sector's number, xored with the tweak key, as the IV */
int CBC_Encrypt_ext(const void *key, const void *tweakKey, SceUInt64 base, SceSize size,
		    SceUInt32 sectorSize, const void *src, void *dst, SceUInt16 flags, SceUInt16 slot)
{
	SceUInt8 block[0x10];
	SceUInt8 *tweak, *p;
	SceUInt64 sector;
	SceSize remaining, offset;
	int ret;

	sector = base;
	tweak = block;
	do {
		*tweak++ = (SceUInt8)sector;
		sector >>= 8;
	} while (tweak != block + 8);
	do {
		*tweak++ = 0;
	} while (tweak != block + 0x10);
	remaining = size;
	offset = 0;
	while (size > offset) {
		SceSize len = sectorSize < remaining ? sectorSize : remaining;
		const SceUInt8 *in = (const SceUInt8 *)src + offset;
		SceUInt8 *out = (SceUInt8 *)dst + offset;
		int i;

		/* the sector's number, and the tweak key, make the IV */
		p = block;
		do {
			*p++ = (SceUInt8)sector;
			sector >>= 8;
		} while (p != block + 8);
		do {
			*p++ = 0;
		} while (p != block + 0x10);
		for (i = 0; i < 0x10; i++)
			block[i] ^= ((const SceUInt8 *)tweakKey)[i];
		if ((flags & 1) != 0) {
			out = g_pfsSectorScratch;
			if ((flags & 2) != 0)
				ret = pfsSectorCryptCtrSlotEncrypt(key, block, len, in, out, slot);
			else
				ret = pfsSectorCryptCtrEncrypt(key, block, len, in, out);
		} else if ((flags & 2) != 0) {
			ret = CBC_EncryptNP(key, block, len, in, out, slot);
		} else {
			ret = CBC_Encrypt(key, block, len, in, out);
		}
		if (ret != 0)
			return ret;
		sector = base + offset + sectorSize;
		remaining -= sectorSize;
		offset += sectorSize;
	}
	if ((flags & 1) != 0 && dst != src)
		memcpy(dst, src, size);
	return 0;
}

int CBC_Decrypt_ext(const void *key, const void *tweakKey, SceUInt64 base, SceSize size,
		    SceUInt32 sectorSize, const void *src, void *dst, SceUInt16 flags, SceUInt16 slot)
{
	SceUInt8 block[0x10];
	SceUInt8 *tweak, *p;
	SceUInt64 sector;
	SceSize remaining, offset;
	int ret;

	sector = base;
	tweak = block;
	do {
		*tweak++ = (SceUInt8)sector;
		sector >>= 8;
	} while (tweak != block + 8);
	do {
		*tweak++ = 0;
	} while (tweak != block + 0x10);
	remaining = size;
	offset = 0;
	while (size > offset) {
		SceSize len = sectorSize < remaining ? sectorSize : remaining;
		const SceUInt8 *in = (const SceUInt8 *)src + offset;
		SceUInt8 *out = (SceUInt8 *)dst + offset;
		int i;

		p = block;
		do {
			*p++ = (SceUInt8)sector;
			sector >>= 8;
		} while (p != block + 8);
		do {
			*p++ = 0;
		} while (p != block + 0x10);
		for (i = 0; i < 0x10; i++)
			block[i] ^= ((const SceUInt8 *)tweakKey)[i];
		if ((flags & 1) != 0) {
			out = g_pfsSectorScratch;
			if ((flags & 2) != 0)
				ret = pfsSectorCryptCtrSlotDecrypt(key, block, len, in, out, slot);
			else
				ret = pfsSectorCryptCtrDecrypt(key, block, len, in, out);
		} else if ((flags & 2) != 0) {
			ret = CBC_DecryptNP(key, block, len, in, out, slot);
		} else {
			ret = CBC_Decrypt(key, block, len, in, out);
		}
		if (ret != 0)
			return ret;
		sector = base + offset + sectorSize;
		remaining -= sectorSize;
		offset += sectorSize;
	}
	if ((flags & 1) != 0 && dst != src)
		memcpy(dst, src, size);
	return 0;
}
