/*
 * PFS's cryptographic routines. Every one is deferred:
 * each keeps the original's interface
 * and does no cryptography.
 */

#include <sysmem/sysclib.h>
#include "pfsmgr_internal.h"

void pfsHashCopy(void *dst, const void *src)
{
	*(ScePfsHash *)dst = *(const ScePfsHash *)src;
}

void pfsHashZero(ScePfsHash *hash)
{
	memset(hash, 0, sizeof(*hash));
}

int pfsHashEqual(const void *a, const void *b)
{
	return timingsafe_memcmp(a, b, PFS_HASH_SIZE) == 0;
}

/* PLACEHOLDER: cryptographic routine, deferred: SHA-1 of data */
int pfsSha1(ScePfsHash *digest, const void *data, SceSize len)
{
	memset(digest, 0, sizeof(*digest));
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred: HMAC-SHA1 of data */
int pfsHmacSha1(ScePfsHash *digest, const ScePfsHash *key, const void *data, SceSize len)
{
	memset(digest, 0, sizeof(*digest));
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred: HMAC-SHA1 of data,
 * through the DMA engine when len is given */
int pfsHmacSha1Dma(ScePfsHash *digest, const ScePfsHash *key, const void *data, SceSize len)
{
	memset(digest, 0, sizeof(*digest));
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred: a key from a counter */
void pfsDeriveCounterKey(ScePfsHash *key, const void *klicensee, SceUInt32 high, SceUInt32 low)
{
	memset(key, 0, sizeof(*key));
}

/* PLACEHOLDER: cryptographic routine, deferred: two keys from a seed */
void pfsDeriveKeyPair(void *key1, void *key2, const void *seed, SceUInt32 salt)
{
	memset(key1, 0, 0x10);
	memset(key2, 0, 0x10);
}

/* PLACEHOLDER: cryptographic routine, deferred: a file's data and
 * tweak keys from the klicensee and the file index */
void pfsDeriveFileKeys(void *dataKey, void *tweakKey, const char *klicensee, SceUInt32 keyParam,
		       SceUInt16 flags, SceUInt32 index, SceUInt16 a6)
{
	memset(dataKey, 0, 0x10);
	memset(tweakKey, 0, 0x10);
}

/* PLACEHOLDER: cryptographic routine, deferred: a file's data and
 * tweak keys from the klicensee and the seed its unicv record keeps */
void pfsDeriveFileKeysSeeded(void *dataKey, void *tweakKey, const char *klicensee,
			     SceUInt16 flags, SceUInt16 a6, const ScePfsHash *seed, SceSize seedSize)
{
	memset(dataKey, 0, 0x10);
	memset(tweakKey, 0, 0x10);
}

/* PLACEHOLDER: cryptographic routine, deferred: a file's data and
 * tweak keys in the modes without per-file keys */
void pfsDeriveSimpleKeys(void *dataKey, void *tweakKey, const char *klicensee,
			 SceUInt32 keyParam, SceUInt32 index)
{
	memset(dataKey, 0, 0x10);
	memset(tweakKey, 0, 0x10);
}

/* PLACEHOLDER: cryptographic routine, deferred: the per-file icv key
 * from the klicensee and the file index */
void pfsDeriveKey(ScePfsHash *key, const void *klicensee, SceUInt32 keyParam, SceUInt32 flags,
		  SceUInt32 index, SceUInt32 keyParam2)
{
	memset(key, 0, PFS_HASH_SIZE);
}
