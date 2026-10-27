/*
 * sealedkey.c: sealed keys, 0x50-byte blobs that hold a key encrypted and
 * signed with a console secret.
 */

#include "post_ss_mgr_internal.h"

/* PLACEHOLDER: cryptographic routine, deferred: decrypts the sealed
 * key encryption key and secret with sceSblSsDecryptWithPortability */
POST_SS_DEFERRED int getSealedKeySecret(void *encKey, void *secret)
{
	SceSblSsMgrForDriver_CD98CC92(encKey, 0, 0x10);
	SceSblSsMgrForDriver_CD98CC92(secret, 0, 0x10);
	return SCE_SBL_ERROR_KEYS_UNAVAILABLE;
}

/* PLACEHOLDER: cryptographic routine, deferred: seals a random key */
POST_SS_DEFERRED int sceSblSsGenerateSealedKey(void *sealedKey)
{
	if (sealedKey == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	SceSblSsMgrForDriver_CD98CC92(sealedKey, 0, 0x50);
	return SCE_SBL_ERROR_KEYS_UNAVAILABLE;
}

/* PLACEHOLDER: cryptographic routine, deferred: checks and decrypts a
 * sealed key */
POST_SS_DEFERRED int sceSblSsDecryptSealedKey(const void *sealedKey, void *key)
{
	if (key == NULL || sealedKey == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	return SCE_SBL_ERROR_KEYS_UNAVAILABLE;
}
