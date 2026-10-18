#include "ssmgr_internal.h"
#include <dmacmgr/dmacmgr.h>
#include <threadmgr/threadmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/utils.h>
#include <sysmem/sysmem.h>
#include <sysmem/debug.h>
#include <intrmgr/syscall.h>
#include <ss_mgr/ss_mgr.h>

uint8_t rngSeed[20];
SceUID rngMutex;
uint8_t rngKey[20];

int SsRngGenerateBlock(uint8_t *result, uint8_t *key, const uint8_t *seed);
const uint8_t rngKeyHmacKey[20] = {
	0xa6, 0xd4, 0x9a, 0xe3, 0x65, 0x41, 0x29, 0x69, 0x65, 0xb5,
	0x9c, 0xed, 0x4d, 0xbc, 0xbc, 0x61, 0x0e, 0x32, 0x17, 0x7a
};
const uint8_t rngSeedHmacKey[20] = {
	0x3b, 0xba, 0x65, 0x0f, 0x43, 0x3c, 0xe3, 0x3d, 0xb6, 0xf1,
	0xd6, 0x0b, 0x3b, 0xaa, 0xa2, 0x97, 0x8a, 0x23, 0xcb, 0xa2
};

int SceSblSsMgrForDriver_AC57F4F0(void *result)
{
	return sceSblDmac5Rnd(result, 64, 1);
}

int _set_key_seed(void *result, SceSize size)
{
	uint8_t key[20], seed[20], block[20];
	uint8_t *output = result;
	SceSize copySize;
	int ret;

	ret = sceKernelLockMutex(rngMutex, 1, NULL);
	SCE_DBG_ASSERT_CHECK_FUNC(ret == 0, 98, "_set_key_seed");
	SsRngGenerateBlock(key, rngKey, rngSeed);
	SsRngGenerateBlock(seed, rngKey, rngSeed);
	sceKernelUnlockMutex(rngMutex, 1);
	while (size) {
		uint8_t *destination = output;

		copySize = size < 20 ? size : 20;
		SsRngGenerateBlock(block, key, seed);
		output += copySize;
		memcpy(destination, block, copySize);
		size -= copySize;
	}
	return 0;
}

int sceSblRngInit(void)
{
	uint32_t random[16];
	SceUID mutex;
	int ret;

	mutex = sceKernelCreateMutex("SceSblRng", 0x2000, 0, NULL);
	rngMutex = mutex;
	SCE_DBG_ASSERT_CHECK(mutex >= 0, 134);
	ret = sceSblDmac5Rnd(random, sizeof(random), 1);
	if (ret) {
		SCE_DBG_PRINTF(0, 141, "%s(%d) %s: sceSblDmac5Rnd() r=0x%x.\n",
			       "rng.c", 141, "sceSblRngInit", ret);
		sceKernelDeleteMutex(rngMutex);
	} else {
		SceKernelUtilsForDriver_29A28957(rngKeyHmacKey, sizeof(rngKeyHmacKey), random,
				   sizeof(random), rngKey);
		random[0] = SceThreadmgrForDriver_47F6DE49();
		SceKernelUtilsForDriver_29A28957(rngSeedHmacKey, sizeof(rngSeedHmacKey), random,
				   sizeof(random), rngSeed);
	}
	return ret;
}

static int __attribute__((noinline, noclone)) rngGetRandomNumberForUser(
	void *result, SceSize size)
{
	SceUInt32 syscallFrame = sceSyscallEnter();
	int ret;

	if (!result || size > 64)
		ret = 0x80010016;
	else
		ret = _set_key_seed(result, size);
	sceSyscallLeave(syscallFrame);
	return ret;
}

/* The syscall copies the first word's byte count from this eight-byte record. */
typedef struct RngUserCopyParam {
	SceSize size;
	uint32_t reserved;
} RngUserCopyParam;

int _sceKernelGetRandomNumber(void *result, SceSize size,
	const RngUserCopyParam *userParam)
{
	RngUserCopyParam param;
	uint8_t *buffer;
	int ret, copyRet;

	ret = SceSysmemForDriver_BC996A7A(&param, userParam, sizeof(param));
	if (ret < 0)
		return ret;
	if (!result)
		return rngGetRandomNumberForUser(NULL, size);
	if (param.size > 64)
		return 0x8002710b;
	buffer = __builtin_alloca(64);
	memset(buffer, 0, 64);
	ret = rngGetRandomNumberForUser(buffer, size);
	copyRet = SceSysmemForDriver_6D88EF8A(result, buffer, param.size);
	if (copyRet < 0)
		return copyRet;
	return ret;
}

void SsRngAdd160(const uint32_t *left, const uint32_t *right, uint32_t *result)
{
	int i = 4;
	unsigned int carry = 0;
	uint32_t larger, sum;

	do {
		larger = left[i] > right[i] ? left[i] : right[i];
		sum = carry + right[i] + left[i];
		result[i] = sum;
		if (larger > sum)
			carry = 1;
		else
			carry = left[i] == UINT32_MAX && right[i] == UINT32_MAX;
	} while (--i >= 0);
}

static inline uint32_t rngRotateLeft(uint32_t value, unsigned int count)
{
	return (value << count) | (value >> (32 - count));
}

static inline __attribute__((always_inline)) uint32_t rngLoadBigEndian(
	const uint8_t *bytes)
{
	return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
		((uint32_t)bytes[2] << 8) | bytes[3];
}

int SsRngSha1Transform(uint32_t *state, const uint8_t *block)
{
	uint32_t words[80];
	uint32_t a, b, c, d, e, next, function, constant;
	unsigned int i;

	for (i = 0; i != 16; i++)
		words[i] = rngLoadBigEndian(&block[i * 4]);
	for (i = 16; i != 80; i++)
		words[i] = rngRotateLeft(words[i - 3] ^ words[i - 8] ^
					words[i - 14] ^ words[i - 16], 1);
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];
	for (i = 0; i != 80; i++) {
		if (i <= 19) {
			constant = 0x5a827999;
			function = ((d ^ c) & b) ^ d;
		} else if (i <= 39) {
			constant = 0x6ed9eba1;
			function = b ^ c ^ d;
		} else if (i <= 59) {
			constant = 0x8f1bbcdc;
			function = ((c | b) & d) | (c & b);
		} else {
			constant = 0xca62c1d6;
			function = b ^ c ^ d;
		}
		next = rngRotateLeft(a, 5) + function + e + constant + words[i];
		e = d;
		d = c;
		c = rngRotateLeft(b, 30);
		b = a;
		a = next;
	}
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	return 0;
}

static inline __attribute__((always_inline)) void rngStoreBigEndian(
	uint8_t *bytes, uint32_t value)
{
	bytes[3] = value;
	bytes[0] = value >> 24;
	bytes[1] = value >> 16;
	bytes[2] = value >> 8;
}

/* Legacy Sony SHA-1 context, before Vita added a cached digest.
 * Layout and field names: PSPSDK user/psputils.h. This local generator only
 * passes h to its own transform; it does not call the Vita block API.
 */
typedef struct SceKernelUtilsSha1Context {
	uint32_t h[5];
	uint16_t usRemains;
	uint16_t usComputed;
	uint64_t ullTotalLen;
	uint8_t buf[64];
} SceKernelUtilsSha1Context;

int SsRngGenerateBlock(uint8_t *result, uint8_t *key, const uint8_t *seed)
{
	uint32_t keyWords[5], seedWords[5], sumWords[5];
	SceKernelUtilsSha1Context context;
	uint8_t block[64];
	unsigned int i;

	keyWords[0] = rngLoadBigEndian(&key[0]);
	keyWords[1] = rngLoadBigEndian(&key[4]);
	keyWords[2] = rngLoadBigEndian(&key[8]);
	keyWords[3] = rngLoadBigEndian(&key[12]);
	keyWords[4] = rngLoadBigEndian(&key[16]);
	seedWords[0] = rngLoadBigEndian(&seed[0]);
	seedWords[1] = rngLoadBigEndian(&seed[4]);
	seedWords[2] = rngLoadBigEndian(&seed[8]);
	seedWords[3] = rngLoadBigEndian(&seed[12]);
	seedWords[4] = rngLoadBigEndian(&seed[16]);
	SsRngAdd160(keyWords, seedWords, sumWords);
	rngStoreBigEndian(&block[0], sumWords[0]);
	rngStoreBigEndian(&block[4], sumWords[1]);
	rngStoreBigEndian(&block[8], sumWords[2]);
	rngStoreBigEndian(&block[12], sumWords[3]);
	rngStoreBigEndian(&block[16], sumWords[4]);
	for (i = 20; i != sizeof(block); i++)
		block[i] = 0;
	context.h[0] = 0x67452301;
	context.h[1] = 0xefcdab89;
	context.h[2] = 0x98badcfe;
	context.h[3] = 0x10325476;
	context.h[4] = 0xc3d2e1f0;
	context.usRemains = 0;
	context.usComputed = 0;
	context.ullTotalLen = 0;
	SsRngSha1Transform(context.h, block);
	seedWords[0] = context.h[0];
	seedWords[1] = context.h[1];
	seedWords[2] = context.h[2];
	seedWords[3] = context.h[3];
	seedWords[4] = context.h[4];
	rngStoreBigEndian(&result[0], seedWords[0]);
	rngStoreBigEndian(&result[4], seedWords[1]);
	rngStoreBigEndian(&result[8], seedWords[2]);
	rngStoreBigEndian(&result[12], seedWords[3]);
	rngStoreBigEndian(&result[16], seedWords[4]);
	SsRngAdd160(keyWords, seedWords, sumWords);
	seedWords[4] = 1;
	seedWords[0] = 0;
	seedWords[1] = 0;
	seedWords[2] = 0;
	seedWords[3] = 0;
	SsRngAdd160(sumWords, seedWords, keyWords);
	rngStoreBigEndian(&key[0], keyWords[0]);
	rngStoreBigEndian(&key[4], keyWords[1]);
	rngStoreBigEndian(&key[8], keyWords[2]);
	rngStoreBigEndian(&key[12], keyWords[3]);
	rngStoreBigEndian(&key[16], keyWords[4]);
	return 0;
}
