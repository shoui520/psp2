/*
 * SceNpCommonPs4: HMAC over a hash described by a table of its functions, and the HMAC-SHA-256 of the NpAsm layer.
 */

#include "../npasm/np_asm.h"
#include "crypto.h"

#define NP_COMMON_PS4_HMAC_ERROR_INVALID_ARGUMENT	-2142760960	/* 0x80415FF0 */

/*SORT-START*/
SceSize npCommonPs4Sha256GetBlockSize(void)
{
	return 64;
}

SceSize npCommonPs4Sha256GetDigestSize(void)
{
	return 32;
}

__attribute__((noinline)) int npCommonPs4Hmac(const void *key, SceSize keySize, const void *data, SceSize dataSize, void *digest,
	const struct NpCommonPs4HashAlgorithm *algorithm)
{
	SceUInt8 inner[64];
	SceUInt8 keyBlock[128];
	SceUInt8 context[276];
	SceSize blockSize;
	SceSize digestSize;
	SceUInt8 *pad;
	SceUInt8 *end;

	blockSize = algorithm->getBlockSize(0);
	if (data == NULL || key == NULL || digest == NULL)
		return NP_COMMON_PS4_HMAC_ERROR_INVALID_ARGUMENT;
	sceClibMemset(keyBlock, 0, sizeof(keyBlock));
	if (blockSize < keySize)
		algorithm->hashKey(key, keySize, keyBlock);
	else
		sceClibMemcpy_safe(keyBlock, key, keySize);
	pad = keyBlock;
	end = &keyBlock[blockSize];
	if (blockSize != 0) {
		do {
			*pad ^= 0x36;
			pad++;
		} while (pad != end);
		algorithm->init(context);
		algorithm->update(context, keyBlock, blockSize);
		algorithm->update(context, data, dataSize);
		algorithm->final(context, inner);
		pad = keyBlock;
		do {
			*pad ^= 0x6A;
			pad++;
		} while (pad != end);
	} else {
		algorithm->init(context);
		algorithm->update(context, keyBlock, 0);
		algorithm->update(context, data, dataSize);
		algorithm->final(context, inner);
	}
	algorithm->init(context);
	algorithm->update(context, keyBlock, blockSize);
	digestSize = algorithm->getDigestSize(context);
	algorithm->update(context, inner, digestSize);
	algorithm->final(context, digest);
	return 0;
}

__attribute__((noinline)) int npCommonPs4HmacInit(void *context, const void *key, SceSize keySize, const struct NpCommonPs4HashAlgorithm *algorithm)
{
	SceSize contextOffset;
	SceSize blockSize;
	SceUInt8 *keyBlock;
	SceUInt8 *pad;
	SceUInt8 *end;

	contextOffset = algorithm->contextOffset;
	blockSize = algorithm->getBlockSize();
	if (key == NULL || context == NULL)
		return NP_COMMON_PS4_HMAC_ERROR_INVALID_ARGUMENT;
	keyBlock = (SceUInt8 *)context + contextOffset;
	sceClibMemset(keyBlock, 0, blockSize);
	if (blockSize >= keySize)
		sceClibMemcpy_safe(keyBlock, key, keySize);
	else
		algorithm->hashKey(key, keySize, keyBlock);
	pad = keyBlock;
	if (blockSize != 0) {
		end = keyBlock + blockSize;
		do {
			*pad ^= 0x36;
			pad++;
		} while (pad != end);
	}
	algorithm->init(context);
	algorithm->update(context, keyBlock, blockSize);
	return 0;
}

__attribute__((noinline)) int npCommonPs4HmacUpdate(void *context, const void *data, SceSize size, const struct NpCommonPs4HashAlgorithm *algorithm)
{
	if (data == NULL || context == NULL)
		return NP_COMMON_PS4_HMAC_ERROR_INVALID_ARGUMENT;
	algorithm->update(context, data, size);
	return 0;
}

__attribute__((noinline)) int npCommonPs4HmacFinal(void *context, void *digest, const struct NpCommonPs4HashAlgorithm *algorithm)
{
	SceUInt8 inner[64];
	SceUInt8 keyBlock[128];
	SceUInt8 outer[276];
	SceSize contextOffset;
	SceSize blockSize;
	SceSize digestSize;
	SceUInt8 *pad;
	SceUInt8 *end;

	contextOffset = algorithm->contextOffset;
	blockSize = algorithm->getBlockSize();
	if (digest == NULL || context == NULL)
		return NP_COMMON_PS4_HMAC_ERROR_INVALID_ARGUMENT;
	algorithm->final(context, inner);
	sceClibMemcpy_safe(keyBlock, (SceUInt8 *)context + contextOffset, blockSize);
	pad = keyBlock;
	end = &keyBlock[blockSize];
	if (blockSize != 0) {
		do {
			*pad ^= 0x6A;
			pad++;
		} while (pad != end);
	}
	algorithm->init(outer);
	algorithm->update(outer, keyBlock, blockSize);
	digestSize = algorithm->getDigestSize(context);
	algorithm->update(outer, inner, digestSize);
	algorithm->final(outer, digest);
	return 0;
}

int npAsmHmacSha256(const void *key, SceSize keySize, const void *data, SceSize dataSize, void *digest)
{
	return npCommonPs4Hmac(key, keySize, data, dataSize, digest, &g_npCommonPs4Sha256Algorithm);
}

int npAsmHmacSha256Init(void *context, const void *key, SceSize keySize)
{
	return npCommonPs4HmacInit(context, key, keySize, &g_npCommonPs4Sha256Algorithm);
}

int npAsmHmacSha256Update(void *context, const void *data, SceSize size)
{
	return npCommonPs4HmacUpdate(context, data, size, &g_npCommonPs4Sha256Algorithm);
}

int npAsmHmacSha256Final(void *context, void *digest)
{
	return npCommonPs4HmacFinal(context, digest, &g_npCommonPs4Sha256Algorithm);
}
/*SORT-END*/

const struct NpCommonPs4HashAlgorithm g_npCommonPs4Sha256Algorithm = {	/* 0x81029ca0 */
	0x90,
	npCommonPs4Sha256GetBlockSize,
	npCommonPs4Sha256GetDigestSize,
	npCommonPs4Sha256HashKey,
	npCommonPs4Sha256Init,
	npCommonPs4Sha256Update,
	npCommonPs4Sha256Final,
};
