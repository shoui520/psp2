/*
 * The SHA-1 hashes an icv or a key goes through, and the key and secret
 * derivations built on them: the software ones, and the DMAC5 engine's
 * (icv_set_hmac is the record's name in 3.200.010-CEXTOOL, which names this
 * file; XTS_EncryptSector_dmac5, XTS_Encrypt_ext and the CBC_* names come
 * from the same records; the rest of the names are Princess-of-Sleeping's,
 * from their pfsmgr rework, not Sony's records).
 */

#include <sysmem/sysclib.h>
#include <sysmem/utils.h>
#include <ss_mgr/ss_mgr.h>
#include "../pfsmgr_internal.h"

/* the hmac keys of the file and icv key derivations, and the CBC IV the
 * counter derivation encrypts its hmac with (names unknown) */
static const SceUInt8 g_pfsFileKeyHmacKey[0x14] = {
	0xe4, 0x62, 0x25, 0x8b, 0x1f, 0x31, 0x21, 0x56, 0x07, 0x45,
	0xdb, 0x62, 0xb1, 0x43, 0x67, 0x23, 0xd2, 0xbf, 0x80, 0xfe,
};

static const SceUInt8 g_pfsIcvKeyHmacKey[0x14] = {
	0xaf, 0xe6, 0x56, 0xbb, 0x3c, 0x17, 0x25, 0x6a, 0x3c, 0x80,
	0x9f, 0x6e, 0x9b, 0xf1, 0x9f, 0xdd, 0x5a, 0x38, 0x85, 0x43,
};

static const ScePfsKey g_pfsCounterKeyIv = {
	0xc30cd274, 0x13c28198, 0x100b77ee, 0xa7bee410,
};

void icv_copy(void *dst, const void *src)
{
	*(ScePfsHash *)dst = *(const ScePfsHash *)src;
}

void icv_clear(ScePfsHash *hash)
{
	memset(hash, 0, sizeof(*hash));
}

int icv_equal(const void *a, const void *b)
{
	return timingsafe_memcmp(a, b, PFS_HASH_SIZE) == 0;
}

int icv_set_sha1(ScePfsHash *digest, const void *data, SceSize len)
{
	sceSha1Digest(data, len, digest);
	return 0;
}

int icv_set_hmac_sw(ScePfsHash *digest, const ScePfsHash *key,
		    const void *data, SceSize len)
{
	sceHmacSha1Digest(key, PFS_HASH_SIZE, data, len, digest);
	return 0;
}

/* the DMAC5 engine runs the hmac when there is data; a zero length is the
 * software one. The engine wants its key and its result on a 64-byte
 * boundary, so each sits in a buffer that has the room to align it. */
int icv_set_hmac(ScePfsHash *digest, const ScePfsHash *key, const void *data, SceSize len)
{
	SceUInt32 keyBlock[0x20];
	SceUInt32 result[0x20];
	SceUInt32 *k = (SceUInt32 *)((SceUInt8 *)keyBlock +
				     (-(SceUInt32)(SceUIntPtr)keyBlock & 0x3f));
	SceUInt32 *r = (SceUInt32 *)((SceUInt8 *)result +
				     (-(SceUInt32)(SceUIntPtr)result & 0x3f));
	int ret;

	ret = 0;
	if (len != 0) {
		/* the key block is the hmac key and its 0xc zero bytes */
		k[2] = 0;
		k[3] = 0;
		k[4] = 0;
		k[5] = 0;
		k[6] = 0;
		k[7] = 0;
		*(ScePfsHash *)k = *key;
		ret = sceSblDmac5Sha1HmacTransform(data, r, len, k, NULL, 1, 0);
		if (ret == 0)
			*(ScePfsHash *)digest = *(ScePfsHash *)r;
	} else {
		sceHmacSha1Digest(key, PFS_HASH_SIZE, data, len, digest);
	}
	return ret;
}

/* the icv key from the klicensee and the file index: the index (with the
 * mount's parameter when it has one) hmac'ed, then encrypted with the
 * klicensee as the key. The engine wants its key, IV and result on a 64-byte
 * boundary, so each sits in a buffer that has the room to align it. */
void pfs_get_secret_np(ScePfsHash *key, const void *klicensee, SceUInt32 keyParam,
			 SceUInt32 index, SceUInt16 slot)
{
	ScePfsHash hmac;
	SceUInt32 ivBlock[0x20];
	SceUInt32 macBlock[0x20];
	SceUInt32 outBlock[0x20];
	SceUInt32 *iv = (SceUInt32 *)((SceUInt8 *)ivBlock +
				      (-(SceUInt32)(SceUIntPtr)ivBlock & 0x3f));
	SceUInt32 *mac = (SceUInt32 *)((SceUInt8 *)macBlock +
				       (-(SceUInt32)(SceUIntPtr)macBlock & 0x3f));
	SceUInt32 *out = (SceUInt32 *)((SceUInt8 *)outBlock +
				       (-(SceUInt32)(SceUIntPtr)outBlock & 0x3f));
	SceUInt32 data[2];

	if (keyParam != 0) {
		data[0] = keyParam;
		data[1] = index;
		icv_set_hmac_sw(&hmac, (const ScePfsHash *)g_pfsIcvKeyHmacKey, data, 8);
	} else {
		data[0] = index;
		icv_set_hmac_sw(&hmac, (const ScePfsHash *)g_pfsIcvKeyHmacKey, data, 4);
	}
	*(ScePfsHash *)mac = hmac;
	*(ScePfsKey *)iv = g_pfsCounterKeyIv;
	CBC_EncryptNP(klicensee, iv, PFS_HASH_SIZE, mac, out, slot);
	*(ScePfsHash *)key = *(ScePfsHash *)out;
}

/* two keys from a seed and a salt: the salted sha1s are chained */
void pfs_derive_keys_sha1(ScePfsHash *key1, ScePfsHash *key2, const void *seed, SceUInt32 salt)
{
	SceUInt32 data[2];
	ScePfsHash result;
	ScePfsHash hash[2];
	SceUInt8 chain[0x28];

	icv_set_sha1(&hash[0], seed, 16);
	data[0] = salt;
	data[1] = 1;
	icv_set_sha1(&hash[1], data, 8);
	*(ScePfsHash *)&chain[0] = hash[0];
	*(ScePfsHash *)&chain[0x14] = hash[1];
	icv_set_sha1(&result, chain, sizeof(chain));
	*key1 = result;
	data[1] = 2;
	icv_set_sha1(&hash[1], data, 8);
	*(ScePfsHash *)&chain[0] = hash[0];
	*(ScePfsHash *)&chain[0x14] = hash[1];
	icv_set_sha1(&result, chain, sizeof(chain));
	*key2 = result;
}

/* a file's data and tweak keys from the klicensee and the file index */
void pfs_get_keys(void *dataKey, void *tweakKey, const char *klicensee, SceUInt32 keyParam,
		       SceUInt16 flags, SceUInt32 index, SceUInt16 a6)
{
	ScePfsHash hmac;
	SceUInt32 data[2];

	if (flags & 2) {
		*(ScePfsKey *)dataKey = *(const ScePfsKey *)klicensee;
		if (keyParam != 0) {
			data[0] = keyParam;
			data[1] = index;
			icv_set_hmac_sw(&hmac, (const ScePfsHash *)g_pfsFileKeyHmacKey, data, 8);
		} else {
			data[0] = index;
			icv_set_hmac_sw(&hmac, (const ScePfsHash *)g_pfsFileKeyHmacKey, data, 4);
		}
		*(ScePfsKey *)tweakKey = *(ScePfsKey *)&hmac;
	} else {
		pfs_derive_keys_sha1(dataKey, tweakKey, klicensee, index);
	}
}

/* a file's data and tweak keys from the klicensee and the seed its unicv
 * record keeps */
void pfs_get_keys_hmac(void *dataKey, void *tweakKey, const char *klicensee,
			     SceUInt16 flags, SceUInt16 a6, const void *seed, SceSize seedSize)
{
	ScePfsHash hmac;

	icv_set_hmac_sw(&hmac, (const ScePfsHash *)g_pfsFileKeyHmacKey, seed, seedSize);
	*(ScePfsKey *)dataKey = *(const ScePfsKey *)klicensee;
	*(ScePfsKey *)tweakKey = *(ScePfsKey *)&hmac;
}

/* a file's data and tweak keys in the modes without per-file keys */
void pfs_get_keys_sha1(void *dataKey, void *tweakKey, const char *klicensee,
			 SceUInt32 keyParam, SceUInt32 index)
{
	pfs_derive_keys_sha1(dataKey, tweakKey, klicensee, index);
}

/* the per-file icv key from the klicensee and the file index */
void pfs_get_secret(ScePfsHash *key, const void *klicensee, SceUInt32 keyParam, SceUInt32 flags,
		  SceUInt32 index, SceUInt16 a6)
{
	SceUInt32 data[2];
	ScePfsHash result;
	ScePfsHash hash[2];

	if (flags & 1) {
		memset(key, 0, sizeof(*key));
		return;
	}
	if (flags & 2) {
		pfs_get_secret_np(key, klicensee, keyParam, index, a6);
		return;
	}
	SceUInt8 chain[0x28];

	data[0] = 10;
	data[1] = index;
	icv_set_sha1(&hash[0], klicensee, 16);
	icv_set_sha1(&hash[1], data, 8);
	*(ScePfsHash *)&chain[0] = hash[0];
	*(ScePfsHash *)&chain[0x14] = hash[1];
	icv_set_sha1(&result, chain, sizeof(chain));
	*key = result;
}
