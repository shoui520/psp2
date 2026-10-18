/*
 * Keystones: the 0x60-byte record of the parental-control passcode, an HMAC
 * of the passcode under a key of the keystone's type, itself signed.
 */

#include "post_ss_mgr_internal.h"

/* PLACEHOLDER: cryptographic routine, deferred: checks a keystone's
 * magic, version and HMAC */
POST_SS_DEFERRED int postSsMgrCheckKeystone(const ScePostSsKeystone *keystone)
{
	return SCE_SBL_ERROR_MISMATCH;
}

/* PLACEHOLDER: cryptographic routine, deferred */
POST_SS_DEFERRED int SceSblPostSsMgrForDriver_42474C8B(const void *src, ScePostSsKeystone *keystone)
{
	if (keystone == NULL || src == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	return SCE_SBL_ERROR_KEYS_UNAVAILABLE;
}

/* PLACEHOLDER: cryptographic routine, deferred */
POST_SS_DEFERRED int SceSblPostSsMgrForDriver_CC5AA5A5(const ScePostSsKeystone *keystone, void *dst)
{
	if (dst == NULL || keystone == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	return SCE_SBL_ERROR_KEYS_UNAVAILABLE;
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

/* PLACEHOLDER: cryptographic routine, deferred: checks the passcode's
 * HMAC against the keystone */
POST_SS_DEFERRED int SceSblPostSsMgrForDriver_F86F1452(const ScePostSsKeystone *keystone,
					       const void *passcode)
{
	if (passcode == NULL || keystone == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	return SCE_SBL_ERROR_MISMATCH;
}
