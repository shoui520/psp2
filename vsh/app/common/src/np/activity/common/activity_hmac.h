/*
 * HMAC-SHA1 (RFC 2104), with which the utility checks the static resource's signature. Sony's
 * file is not named by any check; the name is ours.
 */
#ifndef NP_ACTIVITY_COMMON_ACTIVITY_HMAC_H
#define NP_ACTIVITY_COMMON_ACTIVITY_HMAC_H

#include <scetypes.h>

#define ACTIVITY_SHA1_DIGEST_SIZE	20

/* the HMAC-SHA1 of size bytes of data under a keySize-byte key, into digest */
void activityHmacSha1(const void *key, SceSize keySize, const void *data, SceSize size,
	SceUInt8 *digest);

#endif
