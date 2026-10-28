/*
 * The CBC (and, in this firmware, the CMAC/CTR) ciphers the sector ciphers run:
 * the engine does the whole blocks, and a partial last block is the ECB of the
 * IV or counter xored in.
 *
 * Names: the ones Sony's records give (3.200.010-CEXTOOL), and ours where no
 * record names the function.
 */

#include <sysmem/sysclib.h>
#include <sysmem/aes.h>
#include <sysmem/utils.h>
#include <ss_mgr/ss_mgr.h>
#include "pfsmgr_internal.h"

/* AES-CBC of a sector, with the block the sector's number makes as the IV: the
 * engine does the whole blocks, and a partial last block is the engine's ECB
 * of the IV xored in */
int CBC_Encrypt(const void *key, const void *iv, SceSize size, const void *src, void *dst)
{
	SceUInt8 blockBuf[0x80];
	SceUInt8 *block = blockBuf + (-(SceUInt32)(SceUIntPtr)blockBuf & 0x3f);
	SceSize blocks = size & ~0xf;
	SceUInt8 *out;
	const SceUInt8 *in;
	SceSize tail = size & 0xf;
	SceSize i;
	int ret;

	if (blocks != 0) {
		ret = sceSblDmac5AesCbcEnc(src, dst, blocks, key, 128, (void *)iv, 1);
		if (ret != 0)
			return ret;
	}
	if (tail == 0)
		return 0;
	ret = sceSblDmac5AesEcbEnc(iv, block, 16, key, 128, 1);
	if (ret != 0)
		return ret;
	out = (SceUInt8 *)dst + blocks;
	in = (const SceUInt8 *)src + blocks;
	for (i = 0; i < tail; i++)
		out[i] = in[i] ^ block[i];
	return 0;
}

int CBC_Decrypt(const void *key, const void *iv, SceSize size, const void *src, void *dst)
{
	SceUInt8 blockBuf[0x80];
	SceUInt8 *block = blockBuf + (-(SceUInt32)(SceUIntPtr)blockBuf & 0x3f);
	SceSize blocks = size & ~0xf;
	SceUInt8 *out;
	const SceUInt8 *in;
	SceSize tail = size & 0xf;
	SceSize i;
	int ret;

	if (blocks != 0) {
		ret = sceSblDmac5AesCbcDec(src, dst, blocks, key, 128, (void *)iv, 1);
		if (ret != 0)
			return ret;
	}
	if (tail == 0)
		return 0;
	ret = sceSblDmac5AesEcbEnc(iv, block, 16, key, 128, 1);
	if (ret != 0)
		return ret;
	out = (SceUInt8 *)dst + blocks;
	in = (const SceUInt8 *)src + blocks;
	for (i = 0; i < tail; i++)
		out[i] = in[i] ^ block[i];
	return 0;
}

/* the same with a key slot instead of a key */
int CBC_EncryptNP(const void *key, const void *iv, SceSize size, const void *src, void *dst,
		  SceUInt16 slot)
{
	SceUInt8 blockBuf[0x80];
	SceUInt8 *block = blockBuf + (-(SceUInt32)(SceUIntPtr)blockBuf & 0x3f);
	SceSize blocks = size & ~0xf;
	SceUInt8 *out;
	const SceUInt8 *in;
	SceSize tail = size & 0xf;
	SceSize i;
	int keyId = slot == 1;
	int ret;

	if (blocks != 0) {
		ret = sceSblDmac5AesCbcEncNP(src, dst, blocks, key, 128, (void *)iv, keyId, 1);
		if (ret != 0)
			return ret;
	} else if (tail == 0) {
		return 0;
	}
	ret = sceSblDmac5AesEcbEncNP(iv, block, 16, key, 128, keyId, 1);
	if (ret != 0)
		return ret;
	out = (SceUInt8 *)dst + blocks;
	in = (const SceUInt8 *)src + blocks;
	for (i = 0; i < tail; i++)
		out[i] = in[i] ^ block[i];
	return 0;
}

int CBC_DecryptNP(const void *key, const void *iv, SceSize size, const void *src, void *dst,
		  SceUInt16 slot)
{
	SceUInt8 blockBuf[0x80];
	SceUInt8 *block = blockBuf + (-(SceUInt32)(SceUIntPtr)blockBuf & 0x3f);
	SceSize blocks = size & ~0xf;
	SceUInt8 *out;
	const SceUInt8 *in;
	SceSize tail = size & 0xf;
	SceSize i;
	int keyId = slot == 1;
	int ret;

	if (blocks != 0) {
		ret = SceSblSsMgrForDriver_1901CB5E(src, dst, blocks, key, 128, (void *)iv, keyId, 1);
		if (ret != 0)
			return ret;
	}
	if (tail == 0)
		return 0;
	ret = sceSblDmac5AesEcbEncNP(iv, block, 16, key, 128, keyId, 1);
	if (ret != 0)
		return ret;
	out = (SceUInt8 *)dst + blocks;
	in = (const SceUInt8 *)src + blocks;
	for (i = 0; i < tail; i++)
		out[i] = in[i] ^ block[i];
	return 0;
}

/* AES-CTR of a sector, the counter the sector's number makes: the engine does
 * the whole blocks from it, and a partial last block is its ECB xored in */
int pfsSectorCryptCtrEncrypt(const void *key, const void *iv, SceSize size, const void *src,
			     void *dst)
{
	SceUInt8 blockBuf[0x80];
	SceUInt8 *block = blockBuf + (-(SceUInt32)(SceUIntPtr)blockBuf & 0x3f);
	SceSize blocks = size & ~0xf;
	SceUInt8 *out;
	const SceUInt8 *in;
	SceSize tail = size & 0xf;
	SceSize i;
	int ret;

	if (blocks != 0) {
		ret = SceSblSsMgrForDriver_1B14658D(src, dst, blocks, key, 128, (void *)iv, 1, 0);
		if (ret != 0)
			return ret;
	}
	if (tail == 0)
		return 0;
	ret = sceSblDmac5AesEcbEnc(iv, block, 16, key, 128, 1);
	if (ret != 0)
		return ret;
	out = (SceUInt8 *)dst + blocks;
	in = (const SceUInt8 *)src + blocks;
	for (i = 0; i < tail; i++)
		out[i] = in[i] ^ block[i];
	return 0;
}

int pfsSectorCryptCtrDecrypt(const void *key, const void *iv, SceSize size, const void *src,
			     void *dst)
{
	SceUInt8 blockBuf[0x80];
	SceUInt8 *block = blockBuf + (-(SceUInt32)(SceUIntPtr)blockBuf & 0x3f);
	SceSize blocks = size & ~0xf;
	SceUInt8 *out;
	const SceUInt8 *in;
	SceSize tail = size & 0xf;
	SceSize i;
	int ret;

	if (blocks != 0) {
		ret = SceSblSsMgrForDriver_1B14658D(src, dst, blocks, key, 128, (void *)iv, 1, 0);
		if (ret != 0)
			return ret;
	}
	if (tail == 0)
		return 0;
	ret = sceSblDmac5AesEcbEnc(iv, block, 16, key, 128, 1);
	if (ret != 0)
		return ret;
	out = (SceUInt8 *)dst + blocks;
	in = (const SceUInt8 *)src + blocks;
	for (i = 0; i < tail; i++)
		out[i] = in[i] ^ block[i];
	return 0;
}

int pfsSectorCryptCtrSlotEncrypt(const void *key, const void *iv, SceSize size, const void *src,
				 void *dst, SceUInt16 slot)
{
	SceUInt8 blockBuf[0x80];
	SceUInt8 *block = blockBuf + (-(SceUInt32)(SceUIntPtr)blockBuf & 0x3f);
	SceSize blocks = size & ~0xf;
	SceUInt8 *out;
	const SceUInt8 *in;
	SceSize tail = size & 0xf;
	SceSize i;
	int keyId = slot == 1;
	int ret;

	if (blocks != 0) {
		ret = SceSblSsMgrForDriver_83B058F5(src, dst, blocks, key, 128, (void *)iv, keyId, 1, 0);
		if (ret != 0)
			return ret;
	}
	if (tail == 0)
		return 0;
	ret = sceSblDmac5AesEcbEncNP(iv, block, 16, key, 128, keyId, 1);
	if (ret != 0)
		return ret;
	out = (SceUInt8 *)dst + blocks;
	in = (const SceUInt8 *)src + blocks;
	for (i = 0; i < tail; i++)
		out[i] = in[i] ^ block[i];
	return 0;
}

int pfsSectorCryptCtrSlotDecrypt(const void *key, const void *iv, SceSize size, const void *src,
				 void *dst, SceUInt16 slot)
{
	SceUInt8 blockBuf[0x80];
	SceUInt8 *block = blockBuf + (-(SceUInt32)(SceUIntPtr)blockBuf & 0x3f);
	SceSize blocks = size & ~0xf;
	SceUInt8 *out;
	const SceUInt8 *in;
	SceSize tail = size & 0xf;
	SceSize i;
	int keyId = slot == 1;
	int ret;

	if (blocks != 0) {
		ret = SceSblSsMgrForDriver_83B058F5(src, dst, blocks, key, 128, (void *)iv, keyId, 1, 0);
		if (ret != 0)
			return ret;
	}
	if (tail == 0)
		return 0;
	ret = sceSblDmac5AesEcbEncNP(iv, block, 16, key, 128, keyId, 1);
	if (ret != 0)
		return ret;
	out = (SceUInt8 *)dst + blocks;
	in = (const SceUInt8 *)src + blocks;
	for (i = 0; i < tail; i++)
		out[i] = in[i] ^ block[i];
	return 0;
}

