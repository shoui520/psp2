/*
 * sealedkey.c: sealed keys, 0x50-byte blobs that hold a key encrypted and
 * signed with a console secret.
 */

#include <sysmem/utils.h>
#include "post_ss_mgr_internal.h"

#define SEALED_KEY_MAGIC	"pfsSKKey"
#define SEALED_KEY_ENC_MAGIC	"pfsSKKey__EncKey"
#define SEALED_KEY_SEC_MAGIC	"pfsSKKey__Secret"
#define SEALED_KEY_PORT_TYPE	6

typedef struct SceSealedKey {
	char magic[8];
	SceUInt8 version;
	SceUInt8 versionHigh;
	SceUInt8 reserved[6];
	SceUInt8 iv[0x10];
	SceUInt8 key[0x10];
	SceUInt8 hmac[0x20];
} SceSealedKey;

typedef struct SceSealedKeyCipherContext {
	SceAesCompactContext aes;
	SceUInt8 iv[0x20];
} SceSealedKeyCipherContext;

static SceUInt8 g_sealedKeyV1Secret[0x10];
static SceUInt8 g_sealedKeyV1EncKey[0x10];

static const SceUInt8 g_sealedKeySecIv1[0x10] = {
	0xb3, 0x1e, 0x7e, 0x0e, 0xbf, 0x82, 0xa3, 0xb1,
	0x73, 0x9d, 0x16, 0x43, 0x62, 0x98, 0x27, 0x09,
};

static const ScePortabilityData g_sealedKeySecData0 = {
	.msg_size = 0x20,
	.msg = {
		0x13, 0x3e, 0xf7, 0x73, 0xbb, 0x61, 0xee, 0xc3,
		0x9b, 0x02, 0xe1, 0x46, 0x35, 0x2a, 0xff, 0xdf,
		0x40, 0x3d, 0x4b, 0xf5, 0x85, 0xcd, 0x1e, 0xed,
		0x38, 0x50, 0x6f, 0x17, 0xb1, 0x94, 0xf7, 0x69,
	},
};

static const SceUInt8 g_sealedKeyEncIv0[0x10] = {
	0x57, 0xd4, 0xab, 0x60, 0x5b, 0x5d, 0x35, 0x24,
	0x8a, 0xf4, 0xa9, 0xc2, 0xe2, 0xfd, 0x86, 0x0e,
};

static const ScePortabilityData g_sealedKeyEncData1 = {
	.msg_size = 0x20,
	.msg = {
		0x12, 0xe9, 0x45, 0x3e, 0xcb, 0x8f, 0x21, 0x54,
		0x33, 0xa1, 0x44, 0x13, 0x30, 0x2c, 0xea, 0x1d,
		0x4f, 0xf0, 0x5d, 0xed, 0xe4, 0x30, 0xed, 0x2f,
		0xf3, 0x0f, 0x6d, 0xb7, 0xf6, 0xef, 0xe7, 0x53,
	},
};

static const SceUInt8 g_sealedKeySecIv0[0x10] = {
	0x93, 0x98, 0x0f, 0x23, 0x66, 0x2a, 0x69, 0xaa,
	0x0a, 0x49, 0x3a, 0xd1, 0xbd, 0x37, 0xf4, 0x03,
};

static const ScePortabilityData g_sealedKeyEncData0 = {
	.msg_size = 0x20,
	.msg = {
		0x8b, 0xe4, 0x2b, 0x24, 0x94, 0xf9, 0xf9, 0x71,
		0x94, 0x52, 0x57, 0x3b, 0xbe, 0x0a, 0x36, 0x0d,
		0x3e, 0x8e, 0x13, 0x79, 0x0d, 0x2f, 0xf9, 0x5c,
		0xc6, 0x70, 0xca, 0xb9, 0xa2, 0x8e, 0x9d, 0x42,
	},
};

static const SceUInt8 g_sealedKeyEncIv1[0x10] = {
	0x4e, 0x30, 0x5a, 0x5a, 0x58, 0x75, 0x31, 0xcf,
	0x2f, 0x5e, 0x94, 0x47, 0x7b, 0xba, 0x61, 0x60,
};

static const ScePortabilityData g_sealedKeySecData1 = {
	.msg_size = 0x20,
	.msg = {
		0xe9, 0x79, 0xde, 0xb7, 0x26, 0x49, 0x43, 0x20,
		0x87, 0x72, 0xcc, 0xa7, 0x42, 0x2d, 0x9e, 0xcc,
		0xed, 0xed, 0x98, 0x02, 0x56, 0x69, 0x65, 0xec,
		0xec, 0x33, 0xa8, 0x5c, 0xf2, 0x7f, 0xe7, 0x7f,
	},
};

int getSealedKeySecret(void *encKey, void *secret)
{
	ScePortabilityData out;
	int ret0, ret1;

	SceSblSsMgrForDriver_CD98CC92(encKey, 0, 0x10);
	SceSblSsMgrForDriver_CD98CC92(secret, 0, 0x10);
	SceSblSsMgrForDriver_CD98CC92(&out, 0, sizeof(out));
	out.msg_size = 0x20;
	ret0 = sceSblSsDecryptWithPortability(SEALED_KEY_PORT_TYPE, (void *)g_sealedKeyEncIv0,
					      (ScePortabilityData *)&g_sealedKeyEncData0, &out);
	if (timingsafe_memcmp(out.msg, SEALED_KEY_ENC_MAGIC, 0x10) == 0 && ret0 == 0)
		__builtin_memcpy(encKey, out.msg + 0x10, 0x10);
	else
		ret0 = SCE_SBL_ERROR_KEYS_UNAVAILABLE;

	ret1 = sceSblSsDecryptWithPortability(SEALED_KEY_PORT_TYPE, (void *)g_sealedKeyEncIv1,
					      (ScePortabilityData *)&g_sealedKeyEncData1, &out);
	if (timingsafe_memcmp(out.msg, SEALED_KEY_ENC_MAGIC, 0x10) == 0 && ret1 == 0) {
		__builtin_memcpy(encKey, out.msg + 0x10, 0x10);
		SceSblSsMgrForDriver_CD98CC92(&out, 0, sizeof(out));
	} else {
		SceSblSsMgrForDriver_CD98CC92(&out, 0, sizeof(out));
		if (ret0 != 0)
			return ret0;
	}

	out.msg_size = 0x20;
	ret0 = sceSblSsDecryptWithPortability(SEALED_KEY_PORT_TYPE, (void *)g_sealedKeySecIv0,
					      (ScePortabilityData *)&g_sealedKeySecData0, &out);
	if (timingsafe_memcmp(out.msg, SEALED_KEY_SEC_MAGIC, 0x10) == 0 && ret0 == 0)
		__builtin_memcpy(secret, out.msg + 0x10, 0x10);

	ret1 = sceSblSsDecryptWithPortability(SEALED_KEY_PORT_TYPE, (void *)g_sealedKeySecIv1,
					      (ScePortabilityData *)&g_sealedKeySecData1, &out);
	if (timingsafe_memcmp(out.msg, SEALED_KEY_SEC_MAGIC, 0x10) == 0 && ret1 == 0)
		__builtin_memcpy(secret, out.msg + 0x10, 0x10);

	SceSblSsMgrForDriver_CD98CC92(&out, 0, sizeof(out));
	return 0;
}

int sceSblSsGenerateSealedKey(void *sealedKey)
{
	SceSealedKey *sk = sealedKey;
	union {
		SceSealedKeyCipherContext cipher;
		SceUInt8 hmac[0x20];
	} ctx;
	SceUInt8 encKey[0x10];
	SceUInt8 secret[0x10];
	SceUInt8 block[0x20];
	SceSize n, i;
	int ret;

	if (sk == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	SceSblSsMgrForDriver_CD98CC92(sk, 0, sizeof(*sk));
	ret = getSealedKeySecret(encKey, secret);
	if (ret != 0) {
		SCE_DBG_PRINTF(0, 218, "%s(%d) %s: getSealedKeySecret failed 0x%08x.\n",
			       "sealedkey.c", 218, __func__, ret);
		return ret;
	}
	sk->version = 2;
	sk->versionHigh = 0;
	__builtin_memcpy(sk->magic, SEALED_KEY_MAGIC, sizeof(sk->magic));
	sceSblRngPseudoRandomNumber(sk->iv, sizeof(sk->iv));
	if (SceKernelUtilsForDriver_EDA97D6D(&ctx.cipher.aes, 128, 128, encKey) >= 0) {
		n = ctx.cipher.aes.blockWords * 4;
		for (i = 0; i < n; i++)
			ctx.cipher.iv[i] = sk->iv[i];
	}
	sceSblRngPseudoRandomNumber(encKey, sizeof(encKey));
	n = ctx.cipher.aes.blockWords * 4;
	for (i = 0; i < n; i++)
		block[i] = encKey[i] ^ ctx.cipher.iv[i];
	SceKernelUtilsForDriver_302947B6(&ctx.cipher.aes, block, sk->key);
	n = ctx.cipher.aes.blockWords * 4;
	for (i = 0; i < n; i++)
		ctx.cipher.iv[i] = sk->key[i];
	SceSblSsMgrForDriver_CD98CC92(encKey, 0, sizeof(encKey));
	sceHmacSha256Digest(secret, sizeof(secret), sk, 0x30, ctx.hmac);
	__builtin_memcpy(sk->hmac, ctx.hmac, sizeof(sk->hmac));
	SceSblSsMgrForDriver_CD98CC92(secret, 0, sizeof(secret));
	return 0;
}

int sceSblSsDecryptSealedKey(const void *sealedKey, void *key)
{
	const SceSealedKey *sk = sealedKey;
	union {
		SceSealedKeyCipherContext cipher;
		SceUInt8 hmac[0x20];
	} ctx;
	SceUInt8 encKey[0x10];
	SceUInt8 secret[0x10];
	SceUInt8 block[0x20];
	SceUInt8 *out = key;
	SceUInt32 version;
	SceSize n, i;
	int ret;

	if (key == NULL || sk == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	if (timingsafe_memcmp(sk->magic, SEALED_KEY_MAGIC, sizeof(sk->magic)) != 0)
		return SCE_SBL_ERROR_BAD_MAGIC;
	version = sk->version + ((SceUInt32)sk->versionHigh << 8);
	if (version <= 1) {
		__builtin_memcpy(encKey, g_sealedKeyV1EncKey, sizeof(encKey));
		__builtin_memcpy(secret, g_sealedKeyV1Secret, sizeof(secret));
	} else if (version == 2) {
		ret = getSealedKeySecret(encKey, secret);
		if (ret != 0) {
			SCE_DBG_PRINTF(0, 295, "%s(%d) %s: getSealedKeySecret failed 0x%08x.\n",
				       "sealedkey.c", 295, __func__, ret);
			return ret;
		}
	} else {
		return SCE_SBL_ERROR_BAD_VERSION;
	}
	sceHmacSha256Digest(secret, sizeof(secret), sk, 0x30, ctx.hmac);
	if (timingsafe_memcmp(ctx.hmac, sk->hmac, sizeof(sk->hmac)) != 0) {
		ret = SCE_SBL_ERROR_MISMATCH;
	} else {
		if (SceKernelUtilsForDriver_72408E29(&ctx.cipher.aes, 128, 128, encKey) >= 0) {
			n = ctx.cipher.aes.blockWords * 4;
			for (i = 0; i < n; i++)
				ctx.cipher.iv[i] = sk->iv[i];
		}
		SceKernelUtilsForDriver_E39CD272(&ctx.cipher.aes, sk->key, block);
		n = ctx.cipher.aes.blockWords * 4;
		for (i = 0; i < n; i++) {
			out[i] = block[i] ^ ctx.cipher.iv[i];
			ctx.cipher.iv[i] = sk->key[i];
		}
		ret = 0;
	}
	SceSblSsMgrForDriver_CD98CC92(encKey, 0, sizeof(encKey));
	SceSblSsMgrForDriver_CD98CC92(secret, 0, sizeof(secret));
	return ret;
}
