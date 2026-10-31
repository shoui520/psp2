/*
 * Keystones: the 0x60-byte record of the parental-control passcode, an HMAC
 * of the passcode under a key of the keystone's type, itself signed.
 */

#include <sysmem/utils.h>
#include "post_ss_mgr_internal.h"

#define KEYSTONE_MAGIC		"keystone"

typedef struct SceKeystoneCipherContext {
	SceAesCompactContext aes;
	SceUInt8 iv[0x20];
} SceKeystoneCipherContext;

static const SceUInt8 g_keystoneHmacKey1[0x20] = {
	0x31, 0x0c, 0x2f, 0x2d, 0x70, 0xa6, 0x22, 0x26,
	0xf4, 0x58, 0x2b, 0x4f, 0xf0, 0x3e, 0x24, 0x19,
	0x6e, 0xef, 0x01, 0xef, 0x73, 0xa8, 0x98, 0x1f,
	0x25, 0x04, 0xbd, 0x50, 0x54, 0x9a, 0x47, 0x8f,
};

static const SceUInt8 g_keystonePasscodeKey1[0x20] = {
	0x54, 0x3e, 0x36, 0x8d, 0xf6, 0x62, 0x9d, 0x68,
	0x2c, 0xd5, 0xa4, 0x36, 0x44, 0xb1, 0xb2, 0xd7,
	0x33, 0xf0, 0xf2, 0x96, 0x4d, 0xcd, 0x2c, 0xe8,
	0xe9, 0xcb, 0x95, 0xd7, 0x6c, 0x9d, 0xd9, 0x28,
};

static const SceUInt8 g_keystoneDebugAesKey[0x10] = { 0 };
static const SceUInt8 g_keystonePasscodeKey0[0x20] = { 0 };
static const SceUInt8 g_keystoneHmacKey0[0x20] = { 0 };

/* checks a keystone's magic, version and HMAC */
int postSsMgrCheckKeystone(const ScePostSsKeystone *keystone)
{
	SceUInt16 version, type;
	const void *key;
	SceUInt8 hmac[0x20];

	if (timingsafe_memcmp(keystone->magic, KEYSTONE_MAGIC, sizeof(keystone->magic)) != 0)
		return SCE_SBL_ERROR_BAD_MAGIC;
	version = 2;
	if (timingsafe_memcmp(&keystone->version, &version, sizeof(version)) != 0)
		return SCE_SBL_ERROR_BAD_VERSION;
	type = keystone->type;
	if (type == 0)
		key = g_keystoneHmacKey0;
	else if (type == 1)
		key = g_keystoneHmacKey1;
	else
		return SCE_SBL_ERROR_UNSUPPORTED;
	sceHmacSha256Digest(key, 0x20, keystone, 0x40, hmac);
	if (timingsafe_memcmp(hmac, keystone->hmac, sizeof(hmac)) != 0)
		return SCE_SBL_ERROR_MISMATCH;
	return 0;
}

int SceSblPostSsMgrForDriver_42474C8B(const void *src, ScePostSsKeystone *keystone)
{
	SceKeystoneCipherContext ctx;
	SceUInt8 block[0x20];
	const SceUInt8 *in = src;
	SceSize n, i, j;

	if (keystone == NULL || src == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	SceSblSsMgrForDriver_CD98CC92(keystone, 0, 0x40);
	keystone->version = 1;
	__builtin_memcpy(keystone->magic, KEYSTONE_MAGIC, sizeof(keystone->magic));
	sceSblRngPseudoRandomNumber(keystone->nonce, sizeof(keystone->nonce));
	if (SceKernelUtilsForDriver_EDA97D6D(&ctx.aes, 128, 128, g_keystoneDebugAesKey) >= 0) {
		n = ctx.aes.blockWords * 4;
		for (i = 0; i < n; i++)
			ctx.iv[i] = keystone->nonce[i];
	} else {
		n = ctx.aes.blockWords * 4;
	}
	for (i = 0; i < 0x20; i += 0x10) {
		for (j = 0; j < n; j++)
			block[j] = in[i + j] ^ ctx.iv[j];
		SceKernelUtilsForDriver_302947B6(&ctx.aes, block, keystone->passcodeHmac + i);
		n = ctx.aes.blockWords * 4;
		for (j = 0; j < n; j++)
			ctx.iv[j] = keystone->passcodeHmac[i + j];
	}
	return 0;
}

int SceSblPostSsMgrForDriver_CC5AA5A5(const ScePostSsKeystone *keystone, void *dst)
{
	SceKeystoneCipherContext ctx;
	SceUInt8 block[0x20];
	SceUInt8 *out = dst;
	SceUInt16 version;
	SceSize n, i, j;

	if (dst == NULL || keystone == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	if (timingsafe_memcmp(keystone->magic, KEYSTONE_MAGIC, sizeof(keystone->magic)) != 0)
		return SCE_SBL_ERROR_BAD_MAGIC;
	version = 1;
	if (timingsafe_memcmp(&keystone->version, &version, sizeof(version)) != 0)
		return SCE_SBL_ERROR_BAD_VERSION;
	if (SceKernelUtilsForDriver_72408E29(&ctx.aes, 128, 128, g_keystoneDebugAesKey) >= 0) {
		n = ctx.aes.blockWords * 4;
		for (i = 0; i < n; i++)
			ctx.iv[i] = keystone->nonce[i];
	}
	for (i = 0; i < 0x20; i += 0x10) {
		SceKernelUtilsForDriver_E39CD272(&ctx.aes, keystone->passcodeHmac + i, block);
		n = ctx.aes.blockWords * 4;
		for (j = 0; j < n; j++) {
			SceUInt8 c = keystone->passcodeHmac[i + j];

			out[i + j] = ctx.iv[j] ^ block[j];
			ctx.iv[j] = c;
		}
	}
	return 0;
}

/* the keystone of type, checked */
int SceSblPostSsMgrForDriver_DDA6FA6D(const ScePostSsKeystone *keystone, SceUInt16 type)
{
	if (keystone == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	if (type != keystone->type)
		return SCE_SBL_ERROR_UNSUPPORTED;
	return postSsMgrCheckKeystone(keystone);
}

/* checks the passcode's HMAC against the keystone */
int SceSblPostSsMgrForDriver_F86F1452(const ScePostSsKeystone *keystone,
				      const void *passcode)
{
	SceUInt16 type;
	const void *key;
	SceUInt8 hmac[0x20];
	int ret;

	if (passcode == NULL || keystone == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	ret = postSsMgrCheckKeystone(keystone);
	if (ret != 0)
		return ret;
	type = keystone->type;
	if (type == 0)
		key = g_keystonePasscodeKey0;
	else if (type == 1)
		key = g_keystonePasscodeKey1;
	else
		return SCE_SBL_ERROR_UNSUPPORTED;
	sceHmacSha256Digest(key, 0x20, passcode, 0x20, hmac);
	if (timingsafe_memcmp(hmac, keystone->passcodeHmac, sizeof(hmac)) != 0)
		return SCE_SBL_ERROR_MISMATCH;
	return 0;
}
