/*
 * Keys kept encrypted for portability in the module and decrypted on first
 * use into caches that are wiped on request (names unknown).
 */

#include "post_ss_mgr_internal.h"

#define POST_SS_KEY_SIZE		0x20
#define POST_SS_KEY_SET_A_COUNT		6
#define POST_SS_KEY_SET_B_COUNT		2
#define POST_SS_CLOUD_KEYS_SIZE		0x2a0

/* a key of set A (name unknown) */
typedef struct ScePostSsKey {
	SceUInt32 words[POST_SS_KEY_SIZE / 4];
} ScePostSsKey;

static char g_keySetA0[POST_SS_KEY_SET_A_COUNT * POST_SS_KEY_SIZE];
static char g_keySetA1[POST_SS_KEY_SET_A_COUNT * POST_SS_KEY_SIZE];
static void *g_keySetA;
static char g_keySetB0[POST_SS_KEY_SET_B_COUNT * POST_SS_KEY_SIZE];
static char g_keySetB1[POST_SS_KEY_SET_B_COUNT * POST_SS_KEY_SIZE];
static void *g_keySetB;
static const void *const *g_cloudKeyTable;
static char g_cloudKeys0[POST_SS_CLOUD_KEYS_SIZE];
static char g_cloudKeys1[POST_SS_CLOUD_KEYS_SIZE];
static void *g_cloudKeys;

/* the key sets, encrypted, each key in its own object, twice */
static const SceUInt8 g_keySetAEnc0Part0[0x20] = {
	0x64, 0x62, 0xb8, 0x1e, 0xfb, 0xed, 0xa3, 0xba,
	0x57, 0x1d, 0x8b, 0x05, 0x34, 0xb7, 0x86, 0x9f,
	0x64, 0x0d, 0x93, 0xd9, 0xe0, 0x06, 0x77, 0xeb,
	0x45, 0xa4, 0x94, 0xed, 0x3e, 0x43, 0xe7, 0x6a,
};

static const SceUInt8 g_keySetAEnc0Part1[0x20] = {
	0x17, 0x2c, 0xa1, 0x47, 0x02, 0x28, 0x9d, 0x6a,
	0x89, 0x03, 0xdb, 0x09, 0x70, 0xe9, 0xea, 0x6a,
	0x13, 0x6d, 0xa8, 0x61, 0x49, 0x8e, 0x3c, 0xaa,
	0xbf, 0x8b, 0xaa, 0x7f, 0x4f, 0x2b, 0x7d, 0xc1,
};

static const SceUInt8 g_keySetAEnc0Part2[0x20] = {
	0x42, 0x95, 0x8e, 0x5b, 0xa6, 0x55, 0xce, 0x3c,
	0x77, 0x3e, 0x79, 0x04, 0xea, 0xc6, 0xaf, 0xa2,
	0x25, 0xb7, 0x88, 0x95, 0xf8, 0x19, 0x30, 0x0c,
	0xaf, 0xb5, 0x08, 0xed, 0x79, 0x5b, 0x10, 0x2b,
};

static const SceUInt8 g_keySetAEnc0Part3[0x20] = {
	0xeb, 0xd7, 0xbb, 0x33, 0x5a, 0x6f, 0xdd, 0x3c,
	0xf6, 0xa8, 0xc0, 0x91, 0xde, 0xce, 0xd9, 0x93,
	0x5c, 0xc9, 0x18, 0x59, 0x47, 0x4e, 0xd1, 0x7b,
	0x1a, 0xfd, 0x8f, 0xe7, 0xf0, 0xe3, 0x1e, 0x15,
};

static const SceUInt8 g_keySetAEnc0Part4[0x20] = {
	0x82, 0x41, 0xb3, 0x47, 0x1b, 0x35, 0xe9, 0xaf,
	0x8f, 0x4f, 0x61, 0xe7, 0x7d, 0xb6, 0x32, 0xd6,
	0xb1, 0x35, 0xbd, 0x64, 0xae, 0x02, 0x2d, 0xd4,
	0x3d, 0xc5, 0xd2, 0xaa, 0xa0, 0x86, 0x52, 0xf7,
};

static const SceUInt8 g_keySetAEnc0Part5[0x20] = {
	0xdc, 0x8c, 0x1c, 0x64, 0x92, 0x28, 0x99, 0xd2,
	0x22, 0x03, 0xfc, 0x87, 0xed, 0xf4, 0xc1, 0xcc,
	0x1e, 0xec, 0x4d, 0x59, 0x49, 0x70, 0xb5, 0x1d,
	0x90, 0xeb, 0x72, 0xad, 0x93, 0x70, 0x27, 0x58,
};

static const SceUInt8 g_keySetAEnc1Part0[0x20] = {
	0xae, 0x29, 0x25, 0x56, 0xbe, 0x56, 0x04, 0x27,
	0xfa, 0x1f, 0x66, 0x60, 0xb4, 0xcc, 0xec, 0x84,
	0x5f, 0xe7, 0xd7, 0x28, 0x82, 0xcb, 0x0d, 0x50,
	0x68, 0x18, 0x0a, 0x99, 0xac, 0xee, 0xff, 0x41,
};

static const SceUInt8 g_keySetAEnc1Part1[0x20] = {
	0x0e, 0x1e, 0x7b, 0xc7, 0xc3, 0xa4, 0x60, 0xdc,
	0xaf, 0x27, 0xed, 0x49, 0xc4, 0x5e, 0x6f, 0x70,
	0x18, 0x4f, 0x8e, 0xb0, 0xab, 0x52, 0x79, 0x2c,
	0xff, 0xa3, 0x6f, 0x87, 0xa6, 0x42, 0xba, 0xf2,
};

static const SceUInt8 g_keySetAEnc1Part2[0x20] = {
	0x92, 0x52, 0x0a, 0x72, 0x3a, 0x02, 0x33, 0x9a,
	0x58, 0xd4, 0x06, 0x53, 0xdd, 0xeb, 0xda, 0xbc,
	0xf8, 0x7f, 0xfe, 0x00, 0x2d, 0xb3, 0x8d, 0x89,
	0x27, 0xf6, 0x45, 0x47, 0xa9, 0xc7, 0x57, 0xb0,
};

static const SceUInt8 g_keySetAEnc1Part3[0x20] = {
	0x40, 0xc7, 0x8f, 0x02, 0xf8, 0x2b, 0x76, 0xd1,
	0x86, 0xbc, 0x97, 0x72, 0x30, 0x4d, 0x85, 0x7c,
	0xc8, 0x21, 0x89, 0x18, 0x2f, 0xe2, 0xda, 0x04,
	0xa7, 0xfc, 0x44, 0xe3, 0x92, 0xcb, 0xd0, 0x61,
};

static const SceUInt8 g_keySetAEnc1Part4[0x20] = {
	0xbd, 0x55, 0x83, 0x67, 0x96, 0x50, 0x7d, 0xb9,
	0x2d, 0x4c, 0xa8, 0x32, 0x1c, 0x02, 0x5e, 0x2e,
	0xe7, 0x9a, 0xba, 0xc7, 0x09, 0xfb, 0x5d, 0xce,
	0x94, 0x1d, 0x09, 0x2c, 0x2d, 0x96, 0x3b, 0xb0,
};

static const SceUInt8 g_keySetAEnc1Part5[0x20] = {
	0xb3, 0x3f, 0x6a, 0xbd, 0x0a, 0xc2, 0x68, 0x35,
	0x9e, 0x4e, 0xb1, 0x7a, 0xcf, 0x44, 0xfe, 0xf8,
	0x4b, 0xa6, 0x09, 0x56, 0x05, 0xb3, 0x04, 0x1f,
	0xd8, 0x50, 0xee, 0xa7, 0xb0, 0x3d, 0xd8, 0xd1,
};

static const SceUInt8 g_keySetBEnc0Part0[0x20] = {
	0x02, 0x77, 0x63, 0x08, 0x86, 0x24, 0x7c, 0x35,
	0x3a, 0x8e, 0xd4, 0x5e, 0xd4, 0x59, 0x77, 0xd2,
	0x72, 0x7b, 0xbb, 0xc5, 0x9e, 0x1d, 0x7e, 0xd4,
	0x3e, 0xc8, 0x77, 0x58, 0x47, 0x5c, 0x42, 0xf0,
};

static const SceUInt8 g_keySetBEnc0Part1[0x20] = {
	0x6b, 0xf7, 0xdf, 0xcb, 0x47, 0x60, 0xce, 0x89,
	0x8a, 0xc1, 0x14, 0x2f, 0x01, 0xee, 0x3e, 0x85,
	0x84, 0x99, 0x66, 0x27, 0xe6, 0x18, 0xcf, 0x17,
	0xee, 0x6f, 0xbe, 0x96, 0x08, 0x3f, 0x43, 0x54,
};

static const SceUInt8 g_keySetBEnc1Part0[0x20] = {
	0xdc, 0xf2, 0x1a, 0x20, 0x7f, 0xa5, 0x4e, 0xef,
	0x3f, 0xfd, 0xf5, 0x67, 0x33, 0x25, 0x7c, 0xe5,
	0x51, 0x5c, 0xd4, 0xb6, 0xe9, 0x52, 0x6a, 0x42,
	0xf9, 0x8b, 0x83, 0x56, 0x95, 0xbf, 0x0c, 0xdf,
};

static const SceUInt8 g_keySetBEnc1Part1[0x20] = {
	0xc0, 0x62, 0x34, 0x40, 0x98, 0xd0, 0xb9, 0xae,
	0xc8, 0x0e, 0xaf, 0x59, 0xa2, 0x37, 0x16, 0x9f,
	0xe1, 0x9c, 0x9a, 0xea, 0xcd, 0xb5, 0x84, 0x5d,
	0x65, 0x4a, 0x97, 0xc6, 0x26, 0x1a, 0x63, 0xa1,
};

static const SceUInt8 g_keySetAIv0[0x10] = {
	0x12, 0x00, 0xb9, 0x3a, 0xd7, 0xdf, 0xbe, 0x0a,
	0x12, 0x7e, 0x65, 0xc1, 0x31, 0xdd, 0x6c, 0x65,
};

static const SceUInt8 g_keySetAIv1[0x10] = {
	0x24, 0x01, 0x6e, 0x27, 0x05, 0x26, 0xc2, 0x5a,
	0xd8, 0x09, 0xfc, 0x50, 0x8f, 0x54, 0xb7, 0x1e,
};

static const SceUInt8 g_keySetBIv0[0x10] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const SceUInt8 g_keySetBIv1[0x10] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const void *const g_keySetAEnc0[POST_SS_KEY_SET_A_COUNT] = {
	g_keySetAEnc0Part0,
	g_keySetAEnc0Part1,
	g_keySetAEnc0Part2,
	g_keySetAEnc0Part3,
	g_keySetAEnc0Part4,
	g_keySetAEnc0Part5,
};

static const void *const g_keySetAEnc1[POST_SS_KEY_SET_A_COUNT] = {
	g_keySetAEnc1Part0,
	g_keySetAEnc1Part1,
	g_keySetAEnc1Part2,
	g_keySetAEnc1Part3,
	g_keySetAEnc1Part4,
	g_keySetAEnc1Part5,
};

static const void *const g_keySetBEnc0[POST_SS_KEY_SET_B_COUNT] = {
	g_keySetBEnc0Part0,
	g_keySetBEnc0Part1,
};

static const void *const g_keySetBEnc1[POST_SS_KEY_SET_B_COUNT] = {
	g_keySetBEnc1Part0,
	g_keySetBEnc1Part1,
};

/* where the 0x20-byte keys of types 1 to 3 are in set A */
static const SceUInt8 g_keySetAOffsets[3] = { 0x20, 0x40, 0x60 };
/* where the 0x10-byte keys are, after the 0x80 bytes of 0x20-byte keys in set A */
static const SceUInt8 g_keySetAKeyOffsets[4] = { 0x00, 0x10, 0x20, 0x30 };
static const SceUInt8 g_keySetBKeyOffsets[2] = { 0x20, 0x30 };

/* PLACEHOLDER: cryptographic routine, deferred: decrypts the two copies
 * of a key set with SceSblSsMgrForDriver_934DB6B5 and keeps the copy that checks */
POST_SS_DEFERRED int postSsMgrDecryptKeySet(const void *const *encA, const void *ivA, const void *const *encB,
			   const void *ivB, SceSize count, void *outA, void *outB, void **keys)
{
	/* as when neither copy checks */
	SceSblSsMgrForDriver_CD98CC92(outA, 0xff, count * POST_SS_KEY_SIZE);
	sceKernelDcacheCleanRange(outA, count * POST_SS_KEY_SIZE);
	SceSblSsMgrForDriver_CD98CC92(outB, 0xff, count * POST_SS_KEY_SIZE);
	sceKernelDcacheCleanRange(outB, count * POST_SS_KEY_SIZE);
	return SCE_SBL_ERROR_KEYS_UNAVAILABLE;
}

void *postSsMgrGetKeySetA(void)
{
	void *keys = g_keySetA;

	if (keys == NULL &&
	    postSsMgrDecryptKeySet(g_keySetAEnc0, g_keySetAIv0, g_keySetAEnc1, g_keySetAIv1,
				   POST_SS_KEY_SET_A_COUNT, g_keySetA0, g_keySetA1, &g_keySetA) != 0) {
		g_keySetA = keys;
		return NULL;
	}
	return g_keySetA;
}

void *postSsMgrGetKeySetB(void)
{
	void *keys = g_keySetB;

	if (keys == NULL &&
	    postSsMgrDecryptKeySet(g_keySetBEnc0, g_keySetBIv0, g_keySetBEnc1, g_keySetBIv1,
				   POST_SS_KEY_SET_B_COUNT, g_keySetB0, g_keySetB1, &g_keySetB) != 0) {
		g_keySetB = keys;
		return NULL;
	}
	return g_keySetB;
}

/* decrypt both key sets */
int SceSblPostSsMgrForDriver_C977601E(void)
{
	int ret;

	if (lockFastMutex(&g_postSsMgrKeysMutex) != 0)
		return SCE_SBL_ERROR_KEYS_LOCK;
	if (postSsMgrGetKeySetA() != NULL && postSsMgrGetKeySetB() != NULL) {
		ret = sceKernelUnlockFastMutex(&g_postSsMgrKeysMutex);
		return ret < 0 ? ret : 0;
	}
	sceKernelUnlockFastMutex(&g_postSsMgrKeysMutex);
	return SCE_SBL_ERROR_KEYS_UNAVAILABLE;
}

/* wipe both key sets */
int SceSblPostSsMgrForDriver_4100B9E9(void)
{
	int ret;

	ret = lockFastMutex(&g_postSsMgrKeysMutex);
	if (ret < 0)
		return ret;
	SceSblSsMgrForDriver_CD98CC92(g_keySetA0, 0xff, sizeof(g_keySetA0));
	SceSblSsMgrForDriver_CD98CC92(g_keySetA1, 0xff, sizeof(g_keySetA1));
	g_keySetA = NULL;
	sceKernelDcacheCleanRange(g_keySetA0, sizeof(g_keySetA0));
	sceKernelDcacheCleanRange(g_keySetA1, sizeof(g_keySetA1));
	SceSblSsMgrForDriver_CD98CC92(g_keySetB0, 0xff, sizeof(g_keySetB0));
	SceSblSsMgrForDriver_CD98CC92(g_keySetB1, 0xff, sizeof(g_keySetB1));
	g_keySetB = NULL;
	sceKernelDcacheCleanRange(g_keySetB0, sizeof(g_keySetB0));
	sceKernelDcacheCleanRange(g_keySetB1, sizeof(g_keySetB1));
	ret = sceKernelUnlockFastMutex(&g_postSsMgrKeysMutex);
	return ret < 0 ? ret : 0;
}

/* a 0x20-byte key of set A, types 1 to 3 */
int SceSblPostSsMgrForDriver_2C463AF1(SceUInt32 type, SceSize size, void *key)
{
	char *keys;
	int ret;

	if (type - 1 >= 3 || size != POST_SS_KEY_SIZE || key == NULL)
		return SCE_SBL_ERROR_KEYS_INVALID_ARGUMENT;
	if (lockFastMutex(&g_postSsMgrKeysMutex) != 0)
		return SCE_SBL_ERROR_KEYS_LOCK;
	keys = postSsMgrGetKeySetA();
	if (keys != NULL) {
		*(ScePostSsKey *)key = *(const ScePostSsKey *)(keys + g_keySetAOffsets[type - 1]);
		ret = sceKernelUnlockFastMutex(&g_postSsMgrKeysMutex);
		return ret < 0 ? ret : 0;
	}
	sceKernelUnlockFastMutex(&g_postSsMgrKeysMutex);
	return SCE_SBL_ERROR_KEYS_UNAVAILABLE;
}

/* a 0x10-byte key: types 1 to 4 of set A, 0x10000001 and 0x10000002 of set B */
int SceSblPostSsMgrForDriver_CB5436BD(SceUInt32 type, SceSize size, void *key)
{
	char *keys;
	SceUInt32 offset;
	int setB, ret;

	if (type < 0x10000000) {
		type -= 1;
		if (type >= 4 || size != 0x10)
			return SCE_SBL_ERROR_KEYS_INVALID_ARGUMENT;
		setB = 0;
	} else {
		type -= 0x10000001;
		if (type >= 2 || size != 0x10)
			return SCE_SBL_ERROR_KEYS_INVALID_ARGUMENT;
		setB = 1;
	}
	if (key == NULL)
		return SCE_SBL_ERROR_KEYS_INVALID_ARGUMENT;
	if (lockFastMutex(&g_postSsMgrKeysMutex) != 0)
		return SCE_SBL_ERROR_KEYS_LOCK;
	if (setB) {
		keys = postSsMgrGetKeySetB();
		if (keys == NULL)
			goto unavailable;
		offset = g_keySetBKeyOffsets[type];
	} else {
		keys = postSsMgrGetKeySetA();
		if (keys == NULL)
			goto unavailable;
		offset = g_keySetAKeyOffsets[type] + 0x80;
	}
	memcpy(key, keys + offset, size);
	ret = sceKernelUnlockFastMutex(&g_postSsMgrKeysMutex);
	return ret < 0 ? ret : 0;

unavailable:
	sceKernelUnlockFastMutex(&g_postSsMgrKeysMutex);
	return SCE_SBL_ERROR_KEYS_UNAVAILABLE;
}

/* PLACEHOLDER: cryptographic routine, deferred: decrypts the two copies
 * of the cloud backup ("CloudBU") keys and keeps the copy that checks */
POST_SS_DEFERRED int postSsMgrLoadCloudKeys(void)
{
	if (g_cloudKeys != NULL && g_cloudKeyTable != NULL)
		return 0;
	/* as when neither copy checks */
	SceSblSsMgrForDriver_CD98CC92(g_cloudKeys0, 0xff, sizeof(g_cloudKeys0));
	sceKernelDcacheCleanRange(g_cloudKeys0, sizeof(g_cloudKeys0));
	SceSblSsMgrForDriver_CD98CC92(g_cloudKeys1, 0xff, sizeof(g_cloudKeys1));
	sceKernelDcacheCleanRange(g_cloudKeys1, sizeof(g_cloudKeys1));
	return SCE_SBL_ERROR_KEYS_UNAVAILABLE;
}

/* the cloud backup keys */
int SceSblPostSsMgrForDriver_C6684F7E(void *keys)
{
	int ret;

	if (keys == NULL)
		return SCE_SBL_ERROR_KEYS_INVALID_ARGUMENT;
	if (lockFastMutex(&g_postSsMgrCloudMutex) != 0)
		return SCE_SBL_ERROR_KEYS_LOCK;
	ret = postSsMgrLoadCloudKeys();
	if (ret < 0) {
		sceKernelUnlockFastMutex(&g_postSsMgrCloudMutex);
		return ret;
	}
	memcpy(keys, g_cloudKeys, POST_SS_CLOUD_KEYS_SIZE);
	return sceKernelUnlockFastMutex(&g_postSsMgrCloudMutex);
}

/* the cloud backup key pair of index (version 1 only) */
int SceSblPostSsMgrForDriver_739C981E(SceUInt32 version, SceUInt32 index, void *keys)
{
	const char *key;
	int ret;

	if (version != 1 || index > 1 || keys == NULL)
		return SCE_SBL_ERROR_KEYS_INVALID_ARGUMENT;
	if (lockFastMutex(&g_postSsMgrCloudMutex) != 0)
		return SCE_SBL_ERROR_KEYS_LOCK;
	ret = postSsMgrLoadCloudKeys();
	if (ret < 0) {
		sceKernelUnlockFastMutex(&g_postSsMgrCloudMutex);
		return ret;
	}
	key = g_cloudKeyTable[index];
	postSsCopyKey(keys, 0x40, key, *(const SceUInt32 *)(key + 0x100));
	key = g_cloudKeyTable[index];
	postSsCopyKey((char *)keys + 0x104, 0x40, key + 0x104, *(const SceUInt32 *)(key + 0x204));
	key = g_cloudKeyTable[index];
	*(SceUInt32 *)((char *)keys + 0x100) = *(const SceUInt32 *)(key + 0x100);
	*(SceUInt32 *)((char *)keys + 0x204) = *(const SceUInt32 *)(key + 0x204);
	return sceKernelUnlockFastMutex(&g_postSsMgrCloudMutex);
}

/* wipe the cloud backup keys */
int SceSblPostSsMgrForDriver_2BF04B8E(void)
{
	int ret;

	ret = lockFastMutex(&g_postSsMgrCloudMutex);
	if (ret < 0)
		return ret;
	if (g_cloudKeys != NULL) {
		SceSblSsMgrForDriver_CD98CC92(g_cloudKeys0, 0xff, sizeof(g_cloudKeys0));
		SceSblSsMgrForDriver_CD98CC92(g_cloudKeys1, 0xff, sizeof(g_cloudKeys1));
		g_cloudKeys = NULL;
		sceKernelDcacheCleanRange(g_cloudKeys0, sizeof(g_cloudKeys0));
		sceKernelDcacheCleanRange(g_cloudKeys1, sizeof(g_cloudKeys1));
	}
	if (g_cloudKeyTable != NULL)
		g_cloudKeyTable = NULL;
	ret = sceKernelUnlockFastMutex(&g_postSsMgrCloudMutex);
	return ret < 0 ? ret : 0;
}

/* PLACEHOLDER: cryptographic routine, deferred: decrypts data with the
 * SD key, decrypted for the call and wiped after */
POST_SS_DEFERRED int SceSblPostSsMgrForDriver_2646DE64(const SceUInt32 *src, SceUInt32 *dst)
{
	if (dst == NULL || src == NULL || (*src != 0x20 && *src != 0x10))
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	return SCE_SBL_ERROR_KEYS_UNAVAILABLE;
}
