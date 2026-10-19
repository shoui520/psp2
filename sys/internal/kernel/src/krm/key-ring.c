#include "krm_internal.h"

int KrmInitializeStateKeys(void)
{
	KrmSha256Digest digest;
	const KrmEccPrivateKey *privateKey;
	const KrmEccPublicKey *publicKey;
	int ret;
	int primaryCheck;
	int alternateCheck;

	g_krmKeysInitialized = 0;
	ret = KrmDriverDecryptPortable(&g_krmProvisionedPrimaryKeyIv,
		&g_krmProvisionedPrimaryKey, &g_krmProvisionedPrimaryKey);
	ret |= KrmDriverDecryptPortable(&g_krmProvisionedPrimaryDigestIv,
		&g_krmProvisionedPrimaryDigest, &g_krmProvisionedPrimaryDigest);
	ret |= KrmDriverDecryptPortable(&g_krmProvisionedAlternateKeyIv,
		&g_krmProvisionedAlternateKey, &g_krmProvisionedAlternateKey);
	ret |= KrmDriverDecryptPortable(&g_krmProvisionedAlternateDigestIv,
		&g_krmProvisionedAlternateDigest, &g_krmProvisionedAlternateDigest);
	if (ret != 0)
		goto clear;
	KrmDriverSha256(&digest, &g_krmProvisionedPrimaryKey, sizeof(digest));
	primaryCheck = timingsafe_memcmp(&digest, &g_krmProvisionedPrimaryDigest,
					sizeof(digest));
	KrmDriverSha256(&digest, &g_krmProvisionedAlternateKey, sizeof(digest));
	alternateCheck = timingsafe_memcmp(&digest, &g_krmProvisionedAlternateDigest,
					  sizeof(digest));
	if (primaryCheck) {
		if (alternateCheck)
			goto clear;
		privateKey = &g_krmProvisionedAlternateKey;
		publicKey = &g_krmAlternateStatePublicKey;
	} else {
		privateKey = &g_krmProvisionedPrimaryKey;
		publicKey = &g_krmPrimaryStatePublicKey;
	}
	__builtin_memcpy(&g_krmStatePrivateKey, privateKey, sizeof(*privateKey));
	__builtin_memcpy(&g_krmStatePublicKey, publicKey, sizeof(*publicKey));
	g_krmKeysInitialized = 1;
clear:
	g_krmProvisionedPrimaryKey = (KrmEccPrivateKey){0};
	g_krmProvisionedPrimaryDigest = (KrmSha256Digest){0};
	g_krmProvisionedAlternateKey = (KrmEccPrivateKey){0};
	g_krmProvisionedAlternateDigest = (KrmSha256Digest){0};
	g_krmProvisionedPrimaryKeyIv = (SceKernelOpenPsId){0};
	g_krmProvisionedPrimaryDigestIv = (SceKernelOpenPsId){0};
	g_krmProvisionedAlternateKeyIv = (SceKernelOpenPsId){0};
	g_krmProvisionedAlternateDigestIv = (SceKernelOpenPsId){0};
	return KrmInitializeWrappingKey();
}

void KrmClearState(void)
{
	memset(&g_krmSignedState, 0, sizeof(g_krmSignedState));
	__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
	__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
	sceKrmDriverSetPID(-1);
}

int KrmInitializeState(KrmState *state, const KrmStateConfig *config)
{
	unsigned int invalidPermissions;
	unsigned int version;

	if (!state || !config)
		return (int)0x80a00000;
	version = KrmDriverGetVersion();
	if (config->version > version)
		return (int)0x80a00006;
	if (config->maxDataSize <= 4096 && config->keyCount <= 64) {
		invalidPermissions = config->permissions & ~0x3fU;
		if (invalidPermissions == 0) {
			memset(state, 0, sizeof(*state));
			state->config = *config;
			g_krmDefaultKeyIndex = -1;
			return invalidPermissions;
		}
	}
	return (int)0x80a00005;
}

int KrmSetDefaultKey(KrmState *state, const void *keyId)
{
	int index = 0;
	int ret;

	if (!state || !keyId)
		return (int)0x80a00000;
	if (state->config.keyCount <= 0)
		return (int)0x80a00002;
	for (;;) {
		if (state->keys[index].info.keyWords) {
			ret = timingsafe_memcmp(keyId, state->keys[index].info.id, 16);
			if (ret == 0)
				break;
		}
		if (++index >= state->config.keyCount)
			return (int)0x80a00002;
	}
	if (state->config.keyCount <= index)
		return (int)0x80a00002;
	g_krmDefaultKeyIndex = index;
	return ret;
}

int KrmGetDefaultKey(KrmState *state, void *keyId)
{
	if (!keyId || !state)
		return (int)0x80a00000;
	if (g_krmDefaultKeyIndex < 0 || g_krmDefaultKeyIndex >= state->config.keyCount ||
	    !state->keys[g_krmDefaultKeyIndex].info.keyWords)
		return (int)0x80a00002;
	__builtin_memcpy(keyId, state->keys[g_krmDefaultKeyIndex].info.id, 16);
	return 0;
}

int KrmImportKey(KrmState *state, const void *wrappingKeyId, const KrmKey *key)
{
	KrmKey local;
	SceKernelOpenPsId iv;
	KrmKey *wrappingKey;
	const unsigned char *id = wrappingKeyId;
	SceSize srcSize;
	SceSize dstSize;
	int slot = 0;
	int index;
	int ret;

	if (!state || !key)
		return (int)0x80a00000;
	if (state->config.keyCount <= 0)
		return (int)0x80a00001;
	while (state->keys[slot].info.keyWords) {
		if (++slot == state->config.keyCount)
			return (int)0x80a00001;
	}
	if (!(state->config.permissions & 0x10))
		return (int)0x80a00006;
	if (key->info.version != 1)
		return (int)0x80a00000;
	for (index = 0; index < state->config.keyCount; ++index) {
		if (state->keys[index].info.keyWords &&
		    timingsafe_memcmp(key->info.id, state->keys[index].info.id, 16) == 0)
			return (int)0x80a00004;
	}
	ret = KrmDriverCheckExpiration(&key->info);
	if (ret < 0)
		return ret;
	if (!wrappingKeyId) {
		if (!(state->config.permissions & 0x20))
			return (int)0x80a00006;
		memcpy(&local, key, sizeof(local));
		goto store;
	}
	for (index = 0; index < state->config.keyCount; ++index) {
		if (state->keys[index].info.keyWords &&
		    timingsafe_memcmp(wrappingKeyId, state->keys[index].info.id, 16) == 0)
			break;
	}
	if (index < state->config.keyCount) {
		wrappingKey = &state->keys[index];
	} else {
		if (id[0] != 255 ||
		    id[1] != 255 ||
		    id[2] != 255 ||
		    id[3] != 255 ||
		    id[4] != 255 ||
		    id[5] != 255 ||
		    id[6] != 255 ||
		    id[7] != 255 ||
		    id[8] != 255 ||
		    id[9] != 255 ||
		    id[10] != 255 ||
		    id[11] != 255 ||
		    id[12] != 255 ||
		    id[13] != 255 ||
		    id[14] != 255 ||
		    id[15] != 255)
			return (int)0x80a00002;
		if (!g_krmWrappingKeyInitialized)
			return (int)0x80a0000a;
		wrappingKey = &g_krmWrappingKey;
	}
	if (!(wrappingKey->info.permissions & 0x20))
		return (int)0x80a00006;
	ret = KrmDriverCheckExpiration(&wrappingKey->info);
	if (ret < 0)
		return ret;
	memcpy(&local, key, sizeof(local));
	if (wrappingKey->info.algorithm == 1) {
		srcSize = 128;
		dstSize = 32;
	} else {
		/* Sony passes bits here, unlike the export path's byte count. */
		srcSize = 32 * key->info.keyWords;
		dstSize = srcSize;
	}
	__builtin_memset(&iv, 0, sizeof(iv));
	ret = KrmDriverDecrypt(wrappingKey, iv.id, &local.material,
			       srcSize, &local.material, dstSize);
	if (ret >= 0) {
store:
		memcpy(&state->keys[slot], &local, sizeof(local));
	}
	memset(&local, 0, sizeof(local));
	return ret;
}

int KrmExportKey(KrmState *state, const void *wrappingKeyId,
		 const void *keyId, KrmKey *key)
{
	KrmKey local;
	SceKernelOpenPsId iv;
	KrmKey *selected;
	KrmKey *wrappingKey;
	unsigned char *ivBytes;
	SceSize srcSize;
	SceSize dstSize;
	int index;
	int ret;

	if (!state || !key)
		return (int)0x80a00000;
	if (!keyId) {
		index = g_krmDefaultKeyIndex;
		if (index < 0 || index >= state->config.keyCount ||
		    !state->keys[index].info.keyWords)
			return (int)0x80a00002;
	} else {
		if (state->config.keyCount <= 0)
			return (int)0x80a00002;
		index = 0;
		for (;;) {
			if (state->keys[index].info.keyWords &&
			    timingsafe_memcmp(keyId, state->keys[index].info.id, 16) == 0)
				break;
			if (++index >= state->config.keyCount)
				return (int)0x80a00002;
		}
	}
	selected = &state->keys[index];
	if (!(state->config.permissions & 1)) {
		ret = (int)0x80a00006;
		goto clear;
	}
	ret = KrmDriverCheckExpiration(&selected->info);
	if (ret < 0)
		goto clear;
	memcpy(&local, selected, sizeof(local));
	if (!(local.info.permissions & 1)) {
		ret = (int)0x80a00006;
		goto clear;
	}
	if (local.info.algorithm == 1 && !(local.info.permissions & 4))
		__builtin_memset(&local.material.ecc.privateKey, 0,
				 sizeof(local.material.ecc.privateKey));
	if (!wrappingKeyId) {
		if (!(state->config.permissions & 2) || !(local.info.permissions & 2)) {
			ret = (int)0x80a00006;
			goto clear;
		}
		goto store;
	}
	if (state->config.keyCount <= 0) {
		ret = (int)0x80a00002;
		goto clear;
	}
	index = 0;
	for (;;) {
		if (state->keys[index].info.keyWords &&
		    timingsafe_memcmp(wrappingKeyId, state->keys[index].info.id, 16) == 0)
			break;
		if (++index >= state->config.keyCount) {
			ret = (int)0x80a00002;
			goto clear;
		}
	}
	wrappingKey = &state->keys[index];
	if (!(local.info.permissions & 0x40)) {
		ret = (int)0x80a00006;
		goto clear;
	}
	ret = KrmDriverCheckExpiration(&wrappingKey->info);
	if (ret < 0)
		goto clear;
	if (wrappingKey->info.algorithm != 1) {
		__builtin_memset(&iv, 0, sizeof(iv));
		ivBytes = iv.id;
		srcSize = 4 * local.info.keyWords;
		dstSize = srcSize;
	} else {
		if (local.info.algorithm == 1 || local.info.keyWords != 8) {
			ret = (int)0x80a00000;
			goto clear;
		}
		ivBytes = NULL;
		srcSize = 32;
		dstSize = 64;
	}
	ret = KrmDriverEncrypt(wrappingKey, ivBytes, &local.material,
			       srcSize, &local.material, dstSize);
store:
	if (ret == 0)
		memcpy(key, &local, sizeof(local));
clear:
	memset(&local, 0, sizeof(local));
	return ret;
}

int sceKrmInternalGenerateKey(KrmState *state, const KrmKey *key)
{
	KrmKey *dst;
	int slot;
	int index;
	int attempt;
	int ret;

	if (!state || !key)
		return (int)0x80a00000;
	if (!(state->config.permissions & 8))
		return (int)0x80a00006;
	if (key->info.version != 1 || key->info.keyWords - 4U > 4 ||
	    key->info.algorithm - 1U > 3 || KrmDriverCheckExpiration(&key->info) < 0)
		return (int)0x80a00000;
	for (index = 0; index < state->config.keyCount; ++index) {
		if (state->keys[index].info.keyWords &&
		    timingsafe_memcmp(key->info.id, state->keys[index].info.id, 16) == 0)
			return (int)0x80a00004;
	}
	if (state->config.keyCount <= 0)
		return (int)0x80a00001;
	for (slot = 0; state->keys[slot].info.keyWords; ++slot) {
		if (slot + 1 == state->config.keyCount)
			return (int)0x80a00001;
	}
	dst = &state->keys[slot];
	if (key->info.algorithm == 1) {
		if (key->info.keyWords != 8)
			return (int)0x80a00000;
		ret = KrmDriverGenerateKeyPair(&dst->material.ecc.publicKey,
					       &dst->material.ecc.privateKey);
		if (ret < 0) {
#if FW_DEBUG
			SceDebugForDriver_D9703808(1, 0, &g_krmGenerateEccDebug,
				g_krmGenerateEccError,
				g_krmGenerateKeyName, ret);
#endif
			return (int)0x80a00008;
		}
	} else {
		attempt = 0;
		do {
			ret = KrmDriverGetRandomBytes(dst->material.symmetric, 4 * key->info.keyWords);
			if (ret < 0) {
#if FW_DEBUG
				SceDebugForDriver_D9703808(1, 0, &g_krmGenerateRandomDebug,
					g_krmGenerateRandomError,
					g_krmGenerateKeyName, ret);
#endif
				memset(dst->material.symmetric, 0, 4 * key->info.keyWords);
				return (int)0x80a00008;
			}
			if (key->info.keyWords == 8 && (key->info.permissions & 1) &&
			    KrmDriverValidatePublicKey((const void *)&dst->material) != 0)
				ret = (int)0x80a00008;
		} while ((++attempt <= 15) & (ret != 0));
	}
	if (ret != 0)
		memset(dst, 0, sizeof(*dst));
	else
		dst->info = key->info;
	return ret;
}

int KrmDeleteKey(KrmState *state, const void *keyId)
{
	KrmKey *key;
	int index;

	if (!state || !keyId)
		return (int)0x80a00000;
	if (state->config.keyCount <= 0)
		return (int)0x80a00002;
	index = 0;
	for (;;) {
		if (state->keys[index].info.keyWords &&
		    timingsafe_memcmp(keyId, state->keys[index].info.id, 16) == 0)
			break;
		if (++index >= state->config.keyCount)
			return (int)0x80a00002;
	}
	key = &state->keys[index];
	if (!(key->info.permissions & 0x10))
		return (int)0x80a00006;
	memset(key, 0, sizeof(*key));
	if (g_krmDefaultKeyIndex == index)
		g_krmDefaultKeyIndex = -1;
	return 0;
}

int KrmCryptData(KrmState *state, const void *keyId, const void *src,
		 SceSize srcSize, void *dst, SceSize dstSize, void *iv, int encrypt)
{
	KrmKey *key;
	unsigned int permission;
	int index;
	int ret;

	if (!state || !src)
		return (int)0x80a00000;
	if (!dst)
		return (int)0x80a00000;
	if (state->config.maxDataSize < srcSize || state->config.maxDataSize < dstSize)
		return (int)0x80a00000;
	if (keyId) {
		for (index = 0; index < state->config.keyCount; ++index) {
			if (state->keys[index].info.keyWords &&
			    timingsafe_memcmp(keyId, state->keys[index].info.id, 16) == 0)
				break;
		}
		if (index >= state->config.keyCount)
			return (int)0x80a00002;
	} else {
		index = g_krmDefaultKeyIndex;
		if (index < 0 || index >= state->config.keyCount ||
		    !state->keys[index].info.keyWords)
			return (int)0x80a00002;
	}
	key = &state->keys[index];
	permission = encrypt ? 0x40 : 0x20;
	if (!(key->info.permissions & permission))
		return (int)0x80a00006;
	ret = KrmDriverCheckExpiration(&key->info);
	if (ret >= 0) {
		if (encrypt)
			return KrmDriverEncrypt(key, iv, src, srcSize, dst, dstSize);
		return KrmDriverDecrypt(key, iv, src, srcSize, dst, dstSize);
	}
	return ret;
}

int KrmSignData(KrmState *state, const void *keyId, const void *src,
		 SceSize srcSize, void *signature, SceSize signatureSize, int digest)
{
	KrmKey *key;
	int index;
	int ret;

	if (!state || !src)
		return (int)0x80a00000;
	if (!signature)
		return (int)0x80a00000;
	if (keyId) {
		index = 0;
		key = state->keys;
		while (index < state->config.keyCount) {
			if (key->info.keyWords &&
			    timingsafe_memcmp(keyId, key->info.id, 16) == 0)
				break;
			++index;
			++key;
		}
		if (index >= state->config.keyCount)
			return (int)0x80a00002;
	} else {
		index = g_krmDefaultKeyIndex;
		if (index < 0 || index >= state->config.keyCount ||
		    !state->keys[index].info.keyWords)
			return (int)0x80a00002;
	}
	key = &state->keys[index];
	if (!(key->info.permissions & 0x80))
		return (int)0x80a00006;
	ret = KrmDriverCheckExpiration(&key->info);
	if (ret >= 0) {
		if (digest)
			return KrmDriverSignHash(key, src, srcSize, signature, signatureSize);
		return KrmDriverSign(key, src, srcSize, signature, signatureSize);
	}
	return ret;
}

int KrmVerifyData(KrmState *state, const void *keyId, const void *src,
		 SceSize srcSize, const void *signature, SceSize signatureSize, int digest)
{
	KrmKey *key;
	int index;
	int ret;

	if (!state || !src)
		return (int)0x80a00000;
	if (!signature)
		return (int)0x80a00000;
	if (keyId) {
		index = 0;
		key = state->keys;
		while (index < state->config.keyCount) {
			if (key->info.keyWords &&
			    timingsafe_memcmp(keyId, key->info.id, 16) == 0)
				break;
			++index;
			++key;
		}
		if (index >= state->config.keyCount)
			return (int)0x80a00002;
	} else {
		index = g_krmDefaultKeyIndex;
		if (index < 0 || index >= state->config.keyCount ||
		    !state->keys[index].info.keyWords)
			return (int)0x80a00002;
	}
	key = &state->keys[index];
	if (!(key->info.permissions & 0x100))
		return (int)0x80a00006;
	ret = KrmDriverCheckExpiration(&key->info);
	if (ret >= 0) {
		if (digest)
			return KrmDriverVerifyHash(key, src, srcSize, signature, signatureSize);
		return KrmDriverVerify(key, src, srcSize, signature, signatureSize);
	}
	return ret;
}

int KrmGetKeyInfo(KrmState *state, const void *keyId, KrmKey *result)
{
	KrmKey *key;
	int index;
	int ret;

	if (!result || !state)
		return (int)0x80a00000;
	if (keyId) {
		for (index = 0; index < state->config.keyCount; ++index) {
			if (state->keys[index].info.keyWords &&
			    timingsafe_memcmp(keyId, state->keys[index].info.id, 16) == 0)
				break;
		}
		if (index >= state->config.keyCount)
			return (int)0x80a00002;
	} else {
		index = g_krmDefaultKeyIndex;
		if (index < 0 || index >= state->config.keyCount ||
		    !state->keys[index].info.keyWords)
			return (int)0x80a00002;
	}
	key = &state->keys[index];
	ret = KrmDriverCheckExpiration(&key->info);
	if (ret < 0)
		return ret;
	memset(result, 0, sizeof(*result));
	result->info = key->info;
	return 0;
}

int KrmGetKeyInfoByIndex(KrmState *state, int index, KrmKey *result)
{
	KrmKey *key;
	int ret;

	if (!state || !result)
		return (int)0x80a00000;
	if (index < 0 || index >= state->config.keyCount)
		return (int)0x80a00000;
	key = &state->keys[index];
	if (!key->info.keyWords)
		return (int)0x80a00002;
	ret = KrmDriverCheckExpiration(&key->info);
	if (ret < 0)
		return ret;
	memset(result, 0, sizeof(*result));
	__builtin_memcpy(&result->info, &key->info, offsetof(KrmKeyInfo, id));
	__builtin_memcpy(result->info.id, key->info.id, sizeof(result->info.id));
	return 0;
}

int KrmGetKeyCounts(KrmState *state, KrmKeyCounts *counts)
{
	int index;

	if (!counts || !state)
		return (int)0x80a00000;
	__builtin_memset(counts, 0, sizeof(*counts));
	counts->total = state->config.keyCount;
	for (index = 0; index < counts->total; ++index) {
		if (state->keys[index].info.keyWords)
			++counts->active;
	}
	return 0;
}


int KrmInitializeWrappingKey(void)
{
	KrmSha256Digest digest;
	const KrmSha256Digest *key;
	int ret;
	int primaryCheck;
	int alternateCheck;

	g_krmWrappingKeyInitialized = 0;
	ret = KrmDriverDecryptPortable(g_krmPrimaryWrappingKeyIv,
		&g_krmPrimaryWrappingKey, &g_krmPrimaryWrappingKey);
	ret |= KrmDriverDecryptPortable(g_krmPrimaryWrappingDigestIv,
		&g_krmPrimaryWrappingDigest, &g_krmPrimaryWrappingDigest);
	ret |= KrmDriverDecryptPortable(g_krmAlternateWrappingKeyIv,
		&g_krmAlternateWrappingKey, &g_krmAlternateWrappingKey);
	ret |= KrmDriverDecryptPortable(g_krmAlternateWrappingDigestIv,
		&g_krmAlternateWrappingDigest, &g_krmAlternateWrappingDigest);
	if (ret != 0)
		goto clear;
	KrmDriverSha256(&digest, &g_krmPrimaryWrappingKey, sizeof(digest));
	primaryCheck = timingsafe_memcmp(&digest, &g_krmPrimaryWrappingDigest,
					sizeof(digest));
	KrmDriverSha256(&digest, &g_krmAlternateWrappingKey, sizeof(digest));
	alternateCheck = timingsafe_memcmp(&digest, &g_krmAlternateWrappingDigest,
					  sizeof(digest));
	if (primaryCheck) {
		if (alternateCheck)
			goto clear;
		key = &g_krmAlternateWrappingKey;
	} else {
		key = &g_krmPrimaryWrappingKey;
	}
	g_krmWrappingKeyInitialized = 1;
	__builtin_memcpy(g_krmWrappingKey.material.symmetric, key, sizeof(*key));
clear:
	__builtin_memset(&g_krmPrimaryWrappingKey, 0, sizeof(digest));
	__builtin_memset(&g_krmPrimaryWrappingDigest, 0, sizeof(digest));
	__builtin_memset(&g_krmAlternateWrappingKey, 0, sizeof(digest));
	__builtin_memset(&g_krmAlternateWrappingDigest, 0, sizeof(digest));
	__builtin_memset(g_krmPrimaryWrappingKeyIv, 0, 16);
	__builtin_memset(g_krmPrimaryWrappingDigestIv, 0, 16);
	__builtin_memset(g_krmAlternateWrappingKeyIv, 0, 16);
	__builtin_memset(g_krmAlternateWrappingDigestIv, 0, 16);
	return 0;
}


int KrmCreateSignedNonce(KrmSignedNonce *nonce)
{
	int ret;

	if (!nonce)
		return (int)0x80a00000;
	memset(nonce, 0, sizeof(*nonce));
	ret = KrmDriverGetRandomBytes((unsigned char *)g_krmChallengeNonce, 16);
	if (ret < 0)
		goto clear;
	__builtin_memcpy(nonce->nonce, g_krmChallengeNonce, 16);
	ret = KrmDriverSignMessage(&nonce->signature, nonce->nonce, 16,
				   &g_krmChallengePrivateKey);
	if (__builtin_expect(ret != 0, 0)) {
clear:
		__builtin_memset(g_krmChallengeNonce, 0, 16);
	}
	return ret;
}

int KrmVerifyChallenge(const KrmChallenge *challenge)
{
	int ret;

	if (KrmDriverVerifySignature(&challenge->certificate, &challenge->publicKey,
				    64, &g_krmChallengePublicKey) < 0 ||
	    (ret = KrmDriverVerifySignature(&challenge->signature, challenge,
					    144, &challenge->publicKey)) < 0)
		ret = (int)0x80a0000c;
	else if (timingsafe_memcmp(challenge->nonce, g_krmChallengeNonce, 16) != 0)
		ret = (int)0x80a0000c;
	__builtin_memset(g_krmChallengeNonce, 0, 16);
	return ret;
}

typedef union KrmChallengeBuffer {
	KrmChallengeResponse response;
	KrmEccPublicKey point;
	KrmEccPrivateKey value;
	struct {
		unsigned char nonce[8];
		unsigned int magic;
		unsigned int version;
		SceUInt64 tick;
		unsigned char reserved[7];
		unsigned char counter;
	} seed;
} KrmChallengeBuffer;

int KrmCreateChallengeResponse(const KrmEccPublicKey *publicKey,
			       KrmChallengeResponse *response)
{
	KrmChallengeBuffer *buffer = (KrmChallengeBuffer *)response;
	int count;
	int ret;

	memset(response, 0, sizeof(*response));
	if (KrmDriverGetRandomBytes(buffer->seed.nonce, 8) < 0) {
		ret = (int)0x80a00008;
		goto clear;
	}
	if (KrmDriverGetNetworkTick(&buffer->seed.tick) < 0) {
		ret = (int)0x80a00009;
		goto clear;
	}
	buffer->seed.magic = 1279804243;
	buffer->seed.version = KrmDriverGetVersion();
	count = 64;
	do {
		ret = KrmDriverValidatePublicKey(&buffer->point);
		if (ret == 0)
			break;
		--count;
		++buffer->seed.counter;
	} while (count);
	if (ret < 0)
		goto clear;
	ret = KrmDriverEncodeEcc(&buffer->value, &response->encoded, publicKey);
	if (ret != 0)
		goto clear;
	ret = KrmDriverSignMessage(&response->signature, &response->encoded, 128,
				   &g_krmChallengePrivateKey);
clear:
	__builtin_memset(g_krmChallengeNonce, 0, 16);
	if (ret != 0)
		memset(response, 0, sizeof(*response));
	return ret;
}


int KrmLockChallenge(void)
{
	if (!g_krmChallengeInitialized)
		return (int)0x80a0000a;
	if (KrmDriverGetAuthorization(0) != 0)
		return lockFastMutex(&g_krmChallengeMutex);
	return (int)0x80a00006;
}

int KrmUnlockChallenge(void)
{
	return sceKernelUnlockFastMutex(&g_krmChallengeMutex);
}


int KrmInitializeChallenge(void)
{
	KrmSha256Digest digest;
	const KrmEccPrivateKey *key;
	const KrmEccPublicKey *publicKey;
	int ret;
	int primaryCheck;
	int alternateCheck;

	g_krmChallengeInitialized = 0;
	ret = sceKernelInitializeFastMutex(&g_krmChallengeMutex,
					   g_krmChallengeMutexName, 0, 0);
	ret |= KrmDriverDecryptPortable(g_krmProvisionedChallengePrimaryKeyIv,
		&g_krmProvisionedChallengePrimaryKey, &g_krmProvisionedChallengePrimaryKey);
	ret |= KrmDriverDecryptPortable(g_krmProvisionedChallengePrimaryDigestIv,
		&g_krmProvisionedChallengePrimaryDigest, &g_krmProvisionedChallengePrimaryDigest);
	ret |= KrmDriverDecryptPortable(g_krmProvisionedChallengeAlternateKeyIv,
		&g_krmProvisionedChallengeAlternateKey, &g_krmProvisionedChallengeAlternateKey);
	ret |= KrmDriverDecryptPortable(g_krmProvisionedChallengeAlternateDigestIv,
		&g_krmProvisionedChallengeAlternateDigest, &g_krmProvisionedChallengeAlternateDigest);
	if (ret != 0)
		goto clear;
	KrmDriverSha256(&digest, &g_krmProvisionedChallengePrimaryKey, sizeof(digest));
	primaryCheck = timingsafe_memcmp(&digest, &g_krmProvisionedChallengePrimaryDigest,
					sizeof(digest));
	KrmDriverSha256(&digest, &g_krmProvisionedChallengeAlternateKey, sizeof(digest));
	alternateCheck = timingsafe_memcmp(&digest, &g_krmProvisionedChallengeAlternateDigest,
					  sizeof(digest));
	if (primaryCheck) {
		if (alternateCheck)
			goto clear;
		key = &g_krmProvisionedChallengeAlternateKey;
		publicKey = &g_krmAlternateChallengePublicKey;
	} else {
		key = &g_krmProvisionedChallengePrimaryKey;
		publicKey = &g_krmPrimaryChallengePublicKey;
	}
	g_krmChallengePrivateKey = *key;
	g_krmChallengePublicKey = *publicKey;
	g_krmChallengeInitialized = 1;
clear:
	__builtin_memset(&g_krmProvisionedChallengePrimaryDigest, 0, sizeof(digest));
	__builtin_memset(&g_krmProvisionedChallengePrimaryKey, 0, sizeof(*key));
	__builtin_memset(&g_krmProvisionedChallengeAlternateKey, 0, sizeof(*key));
	__builtin_memset(&g_krmProvisionedChallengeAlternateDigest, 0, sizeof(digest));
	__builtin_memset(g_krmProvisionedChallengePrimaryKeyIv, 0, 16);
	__builtin_memset(g_krmProvisionedChallengePrimaryDigestIv, 0, 16);
	__builtin_memset(g_krmProvisionedChallengeAlternateKeyIv, 0, 16);
	__builtin_memset(g_krmProvisionedChallengeAlternateDigestIv, 0, 16);
	return 0;
}

int KrmDriverInitializeService(void);
int KrmRegisterProcEventHandler(void);

int module_start(SceSize arglen, const void *argp)
{
	if (KrmDriverInitializeService() < 0 || KrmDriverInitialize() < 0 ||
	    KrmInitializeChallenge() < 0 || KrmRegisterProcEventHandler() < 0)
		return 2;
	if (__builtin_expect(KrmInitializeStateKeys() < 0, 0))
		return 2;
	return 0;
}

int KrmProcessKill(SceUID pid, ProcEventParam *param, void *common)
{
	int ret = KrmDriverLock();

	if (__builtin_expect(ret == 0, 1)) {
		if (pid == KrmDriverGetPID()) {
			KrmDriverClearState();
			KrmClearState();
		}
		KrmDriverUnlock();
	}
	return ret;
}

int KrmProcessExit(SceUID pid, ProcEventParam *param, void *common)
{
	int ret = KrmDriverLock();

	if (__builtin_expect(ret == 0, 1)) {
		if (pid == KrmDriverGetPID()) {
			KrmDriverClearState();
			KrmClearState();
		}
		KrmDriverUnlock();
	}
	return ret;
}

int KrmRegisterProcEventHandler(void)
{
	SceKernelProcEventHandler handler = {0};

	handler.size = sizeof(handler);
	handler.exit = KrmProcessExit;
	handler.kill = KrmProcessKill;
	g_krmProcEventHandler = sceKernelRegisterProcEventHandler(
		g_krmProcEventHandlerName, &handler, NULL);
	return g_krmProcEventHandler & (g_krmProcEventHandler >> 31);
}
