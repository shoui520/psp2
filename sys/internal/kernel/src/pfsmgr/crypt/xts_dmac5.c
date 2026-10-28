/*
 * The AES-XTS sector cipher of the DMAC5 engine: the tweak of every 16-byte
 * block (the sector tweak's AES with the tweak key, times x in GF(2^128)) and
 * the engine's ECB over the xored blocks.
 *
 * Names: the ones Sony's records give (3.200.010-CEXTOOL), and ours where no
 * record names the function.
 */

#include <sysmem/sysclib.h>
#include <sysmem/aes.h>
#include <sysmem/utils.h>
#include <ss_mgr/ss_mgr.h>
#include "pfsmgr_internal.h"

/* the XTS tweak step: dst = src xored with the tweak, and the tweak times x
 * in GF(2^128) for the block after it */
void pfsSectorTweakEncrypt(const SceUInt32 *src, const SceUInt32 *tweak, SceUInt32 *dst,
			   SceSize size)
{
	SceUInt64 lo = ((SceUInt64)tweak[1] << 32) | tweak[0];
	SceUInt64 hi = ((SceUInt64)tweak[3] << 32) | tweak[2];

	do {
		SceUInt64 v0 = ((SceUInt64)src[1] << 32) | src[0];
		SceUInt64 v1 = ((SceUInt64)src[3] << 32) | src[2];
		SceUInt32 carry = (SceUInt32)(hi >> 63);

		src += 4;
		dst[0] = (SceUInt32)v0 ^ (SceUInt32)lo;
		dst[1] = (SceUInt32)(v0 >> 32) ^ (SceUInt32)(lo >> 32);
		dst[2] = (SceUInt32)v1 ^ (SceUInt32)hi;
		dst[3] = (SceUInt32)(v1 >> 32) ^ (SceUInt32)(hi >> 32);
		dst += 4;
		hi = (hi << 1) | (lo >> 63);
		lo <<= 1;
		if (carry != 0)
			lo ^= 0x87;
		size -= 0x10;
	} while (size != 0);
}

/* the same xors and the same doubling, but every block xors the tweak it
 * started with, and the buffers do not move */
void pfsSectorTweakDecrypt(const SceUInt32 *src, const SceUInt32 *tweak, SceUInt32 *dst,
			   SceSize size)
{
	do {
		SceUInt64 lo = ((SceUInt64)tweak[1] << 32) | tweak[0];
		SceUInt64 hi = ((SceUInt64)tweak[3] << 32) | tweak[2];
		SceUInt64 v0 = ((SceUInt64)src[1] << 32) | src[0];
		SceUInt64 v1 = ((SceUInt64)src[3] << 32) | src[2];
		SceUInt32 carry = (SceUInt32)(hi >> 63);

		dst[0] = (SceUInt32)v0 ^ (SceUInt32)lo;
		dst[1] = (SceUInt32)(v0 >> 32) ^ (SceUInt32)(lo >> 32);
		dst[2] = (SceUInt32)v1 ^ (SceUInt32)hi;
		dst[3] = (SceUInt32)(v1 >> 32) ^ (SceUInt32)(hi >> 32);
		hi = (hi << 1) | (lo >> 63);
		lo <<= 1;
		if (carry != 0)
			lo ^= 0x87;
		size -= 0x10;
	} while (size != 0);
}

/* one sector's data with the DMAC5 engine, the tweak the sector's number
 * encrypts to mixed in: the tweak is AES-encrypted with the tweak key, the
 * data is xored with it, and the data goes through the engine in place */
int XTS_EncryptSector_dmac5(const void *tweak, const void *key,
				       const void *tweakKey, SceUInt32 keyBits, SceSize size,
				       const void *src, void *dst)
{
	SceAesCompactContext ctx;
	SceUInt32 block[4];
	int ret;

	SceKernelUtilsForDriver_EDA97D6D(&ctx, 128, keyBits, tweakKey);
	SceKernelUtilsForDriver_302947B6(&ctx, tweak, block);
	pfsSectorTweakEncrypt(src, block, dst, size);
	ret = sceSblDmac5AesEcbEnc(dst, dst, size, key, keyBits, 1);
	if (ret == 0)
		pfsSectorTweakEncrypt(dst, block, dst, size);
	return ret;
}

int XTS_DecryptSector_dmac5(const void *tweak, const void *key,
				       const void *tweakKey, SceUInt32 keyBits, SceSize size,
				       const void *src, void *dst)
{
	SceAesCompactContext ctx;
	SceUInt32 block[4];
	int ret;

	SceKernelUtilsForDriver_EDA97D6D(&ctx, 128, keyBits, tweakKey);
	SceKernelUtilsForDriver_302947B6(&ctx, tweak, block);
	pfsSectorTweakEncrypt(src, block, dst, size);
	ret = sceSblDmac5AesEcbDec(dst, dst, size, key, keyBits, 1);
	if (ret == 0)
		pfsSectorTweakEncrypt(dst, block, dst, size);
	return ret;
}
