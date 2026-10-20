/*
 * SceKernelUtils: SHA-1/224/256, HMAC-SHA and AES.
 * The AES block entries are in aes.S, their lookup data in aes_block.c.
 */

#include <scetypes.h>
#include <sysmem/utils.h>
#include <kernel/error.h>
#include "sha2.h"
#include <sysmem/aes.h>
#include <string.h>
#include <fw.h>
#include "sha1_internal.h"
#include "sha2_internal.h"
#include "hmac_internal.h"

extern const uint8_t aesRoundConstants[30];

/* the marker word lands at the byte offset of the buffered data: unaligned, and through
 * the block union's bytes */
typedef uint32_t __attribute__((may_alias)) UnalignedWord;

uint32_t SceKernelUtilsForDriver_60ED6EA9(uint32_t word);
uint32_t SceKernelUtilsForDriver_C76A7685(uint32_t column);

int SceKernelUtilsForDriver_E4390FFA(SceSha1Context *ctx)
{
	if (ctx == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	ctx->h[0] = 0x67452301;
	ctx->h[1] = 0xefcdab89;
	ctx->h[2] = 0x98badcfe;
	ctx->h[3] = 0x10325476;
	ctx->h[4] = 0xc3d2e1f0;
	ctx->ullTotalLen = 0;
	ctx->usRemains = 0;
	ctx->usComputed = 0;
	return 0;
}

int SceKernelUtilsForDriver_478A6F3C(SceSha1Context *ctx, const void *plain, SceSize len)
{
	const uint8_t *input;
	uint32_t block[16];
	uint32_t total, pending;

	if (ctx == NULL || plain == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	input = plain;
	pending = ctx->usRemains;
	total = len + pending;
	ctx->ullTotalLen += len;
	if (__builtin_expect(total > 63, 1)) {
		uint32_t remaining = total;

		do {
			if (pending == 0) {
				do {
					*(uint32_t *)((uint8_t *)block + pending) =
						__builtin_bswap32(*(const uint32_t *)(input + pending));
					pending += sizeof(uint32_t);
				} while (pending != sizeof(block));
				input += sizeof(block);
			} else {
				uint32_t *word = block;
				uint32_t consumed = sizeof(block) - pending;
				int i;

#if FW_SINCE(0x04000000)
				__builtin___memcpy_chk(block, ctx->buf, pending, sizeof(block));
#else
				memcpy(block, ctx->buf, pending);
#endif
				memcpy((uint8_t *)block + ctx->usRemains, input, consumed);
				ctx->usRemains = 0;
				for (i = 0; i < 16; i++)
					word[i] = __builtin_bswap32(word[i]);
				input += consumed;
			}
			remaining -= 64;
			Sha1Transform(block, ctx->h);
			if (remaining <= 63)
				break;
			pending = ctx->usRemains;
		} while (1);
		total &= 63;
	}
	if (total != 0)
		memcpy(ctx->buf + ctx->usRemains, input, total - ctx->usRemains);
	ctx->usRemains = total;
	ctx->usComputed = 0;
	return 0;
}

int SceKernelUtilsForDriver_48F24106(SceSha1Context *ctx, void *result)
{
	uint32_t state[5];
	uint32_t block[16];
	uint8_t *bytes = (uint8_t *)block;
	uint32_t *cache;
	uint32_t *output;
	uint32_t first, second, third, fourth;
	uint32_t *word;

	if (ctx == NULL || result == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	output = result;
	cache = (uint32_t *)ctx->result;
	if (ctx->usComputed == 0) {
		uint32_t pending = ctx->usRemains;
		uint64_t bitLength;
		int shift;

		word = block;
		do {
			*word++ = 0;
		} while (word != block + 16);
		if (pending != 0) {
#if FW_SINCE(0x04000000)
			__builtin___memcpy_chk(block, ctx->buf, pending, sizeof(block));
#else
			memcpy(block, ctx->buf, pending);
#endif
		}
		bytes[pending] = 0x80;
		state[0] = ctx->h[0];
		state[1] = ctx->h[1];
		state[2] = ctx->h[2];
		state[3] = ctx->h[3];
		state[4] = ctx->h[4];
		if (pending > 55) {
			word = block;
			do {
				*word = __builtin_bswap32(*word);
				word++;
			} while (word != block + 16);
			Sha1Transform(block, state);
			word = block;
			do {
				*word++ = 0;
			} while (word != block + 16);
		}
		bitLength = ctx->ullTotalLen << 3;
		bytes += 55;
		for (shift = 56; shift != -8; shift -= 8)
			*++bytes = bitLength >> shift;
		word = block;
		do {
			*word = __builtin_bswap32(*word);
			word++;
		} while (word != block + 16);
		Sha1Transform(block, state);
		word = state;
		do {
			*word = __builtin_bswap32(*word);
			word++;
		} while (word != state + 5);
		first = state[0];
		second = state[1];
		third = state[2];
		fourth = state[3];
		ctx->usComputed = 1;
		cache[0] = first;
		cache[1] = second;
		cache[2] = third;
		cache[4] = state[4];
		cache[3] = fourth;
	}
	first = cache[0];
	second = cache[1];
	third = cache[2];
	fourth = cache[3];
	output[0] = first;
	output[1] = second;
	output[2] = third;
	output[3] = fourth;
	output[4] = cache[4];
	return 0;
}

int SceKernelUtilsForDriver_87DC7F2F(const void *plain, SceSize len, void *digest)
{
	SceSha1Context context;

	SceKernelUtilsForDriver_E4390FFA(&context);
	SceKernelUtilsForDriver_478A6F3C(&context, plain, len);
	SceKernelUtilsForDriver_48F24106(&context, digest);
	return 0;
}

int SceKernelUtilsForDriver_C762EA6D(SceSha224Context *context)
{
	SceKernelSha2Context *ctx = (SceKernelSha2Context *)context;

	if (ctx == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	ctx->ullTotalLen = 0;
	/* The original uses the final buffer byte for count/computed state. */
	ctx->buf[SCE_SHA224_BLOCK_SIZE - 1] = 0;
	__builtin_memcpy(ctx->h, sha224InitialState, sizeof(ctx->h));
	return 0;
}

int SceKernelUtilsForDriver_7556E611(SceSha224Context *context, const void *plain, SceSize len)
{
	SceKernelSha2Context *ctx = (SceKernelSha2Context *)context;
	const unsigned char *input = plain;
	unsigned int pending, needed, blocks;

	if (context == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (plain == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	ctx->ullTotalLen += len;
	pending = ctx->buf[SCE_SHA224_BLOCK_SIZE - 1] & 63;
	if (pending != 0) {
		needed = SCE_SHA224_BLOCK_SIZE - pending;
		if (len < needed)
			goto remainder;
		len -= needed;
		memcpy(ctx->buf + pending, input, needed);
		input += needed;
		Sha256Transform(ctx->buf, 1, ctx->h);
	}
	blocks = len >> 6;
	pending = blocks;
	if (blocks != 0) {
		len &= 63;
		input = Sha256Transform(input, blocks, ctx->h);
		pending = 0;
	}
remainder:
	if (len != 0) {
		memcpy(ctx->buf + pending, input, len);
		pending += len;
	}
	ctx->buf[SCE_SHA224_BLOCK_SIZE - 1] = pending;
	return 0;
}

int SceKernelUtilsForDriver_FBF2A442(SceSha224Context *context, void *result)
{
	SceKernelSha2Context *ctx = (SceKernelSha2Context *)context;
	union {
		unsigned char bytes[2 * SCE_SHA224_BLOCK_SIZE];
		uint32_t words[2 * SCE_SHA224_BLOCK_SIZE / sizeof(uint32_t)];
		uint64_t lengths[2 * SCE_SHA224_BLOCK_SIZE / sizeof(uint64_t)];
	} finalBlocks;
	uint32_t *cached, *output = result;
	unsigned int pending, blocks, i;

	if (result == NULL || ctx == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	cached = ctx->result;
	pending = ctx->buffered.countAndComputed;
	if (pending <= 63) {
		uint32_t first = ctx->h[0], second = ctx->h[1];
		uint32_t third = ctx->h[2], fourth = ctx->h[3];
		uint32_t marker = 0x80;

		cached[0] = first;
		ctx->buffered.countAndComputed = pending + 0x80;
		cached[1] = second;
		cached[2] = third;
		cached[3] = fourth;
		first = ctx->h[4];
		second = ctx->h[5];
		third = ctx->h[6];
		fourth = ctx->h[7];
		cached[4] = first;
		cached[5] = second;
		cached[6] = third;
		cached[7] = fourth;
		memcpy(finalBlocks.bytes, ctx->buf, sizeof(finalBlocks.bytes));
		blocks = (pending + 72) >> 6;
		*(UnalignedWord *)(finalBlocks.bytes + pending) = marker;
		{
			uint32_t *p = (uint32_t *)((uintptr_t)(finalBlocks.bytes + pending + 4) & ~3U);
			unsigned int last = blocks * 16 - 3;

			if ((pending >> 2) < last) {
				unsigned int n = pending >> 2;

				do {
					n++;
					*p++ = 0;
				} while (n != last);
			}
			*(uint64_t *)p = __builtin_bswap64(ctx->ullTotalLen << 3);
		}
		Sha256Transform(finalBlocks.bytes, blocks, cached);
	}
	for (i = 0; i < SCE_SHA224_DIGEST_SIZE / sizeof(uint32_t); i++)
		output[i] = __builtin_bswap32(cached[i]);
	return 0;
}

int SceKernelUtilsForDriver_9EA9D4DC(const void *plain, SceSize len, void *digest)
{
	SceSha224Context context;
	int res;

	SceKernelUtilsForDriver_C762EA6D(&context);
	res = SceKernelUtilsForDriver_7556E611(&context, plain, len);
	if (res >= 0)
		res = SceKernelUtilsForDriver_FBF2A442(&context, digest);
	return res;
}

int SceKernelUtilsForDriver_D909FA2C(SceSha256Context *context)
{
	SceKernelSha2Context *ctx = (SceKernelSha2Context *)context;

	if (ctx == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	ctx->ullTotalLen = 0;
	/* The original uses the final buffer byte for count/computed state. */
	ctx->buf[SCE_SHA256_BLOCK_SIZE - 1] = 0;
	__builtin_memcpy(ctx->h, sha256InitialState, sizeof(ctx->h));
	return 0;
}

int SceKernelUtilsForDriver_236A9097(SceSha256Context *context, const void *plain, SceSize len)
{
	SceKernelSha2Context *ctx = (SceKernelSha2Context *)context;
	const unsigned char *input = plain;
	unsigned int pending, needed, blocks;

	if (context == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (plain == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	ctx->ullTotalLen += len;
	pending = ctx->buf[SCE_SHA256_BLOCK_SIZE - 1] & 63;
	if (pending != 0) {
		needed = SCE_SHA256_BLOCK_SIZE - pending;
		if (len < needed)
			goto remainder;
		len -= needed;
		memcpy(ctx->buf + pending, input, needed);
		input += needed;
		Sha256Transform(ctx->buf, 1, ctx->h);
	}
	blocks = len >> 6;
	pending = blocks;
	if (blocks != 0) {
		len &= 63;
		input = Sha256Transform(input, blocks, ctx->h);
		pending = 0;
	}
remainder:
	if (len != 0) {
		memcpy(ctx->buf + pending, input, len);
		pending += len;
	}
	ctx->buf[SCE_SHA256_BLOCK_SIZE - 1] = pending;
	return 0;
}

int SceKernelUtilsForDriver_4899CD4B(SceSha256Context *context, void *result)
{
	SceKernelSha2Context *ctx = (SceKernelSha2Context *)context;
	union {
		unsigned char bytes[2 * SCE_SHA256_BLOCK_SIZE];
		uint32_t words[2 * SCE_SHA256_BLOCK_SIZE / sizeof(uint32_t)];
		uint64_t lengths[2 * SCE_SHA256_BLOCK_SIZE / sizeof(uint64_t)];
	} finalBlocks;
	uint32_t *cached, *output = result;
	unsigned int pending, blocks, i;

	if (result == NULL || ctx == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	cached = ctx->result;
	pending = ctx->buffered.countAndComputed;
	if (pending <= 63) {
		uint32_t first = ctx->h[0], second = ctx->h[1];
		uint32_t third = ctx->h[2], fourth = ctx->h[3];
		uint32_t marker = 0x80;

		cached[0] = first;
		ctx->buffered.countAndComputed = pending + 0x80;
		cached[1] = second;
		cached[2] = third;
		cached[3] = fourth;
		first = ctx->h[4];
		second = ctx->h[5];
		third = ctx->h[6];
		fourth = ctx->h[7];
		cached[4] = first;
		cached[5] = second;
		cached[6] = third;
		cached[7] = fourth;
		/* Sony copies two blocks before replacing the unused tail. */
		memcpy(finalBlocks.bytes, ctx->buf, sizeof(finalBlocks.bytes));
		blocks = (pending + 72) >> 6;
		*(UnalignedWord *)(finalBlocks.bytes + pending) = marker;
		{
			uint32_t *p = (uint32_t *)((uintptr_t)(finalBlocks.bytes + pending + 4) & ~3U);
			unsigned int last = blocks * 16 - 3;

			if ((pending >> 2) < last) {
				unsigned int n = pending >> 2;

				do {
					n++;
					*p++ = 0;
				} while (n != last);
			}
			*(uint64_t *)p = __builtin_bswap64(ctx->ullTotalLen << 3);
		}
		Sha256Transform(finalBlocks.bytes, blocks, cached);
	}
	for (i = 0; i < SCE_SHA256_DIGEST_SIZE / sizeof(uint32_t); i++)
		output[i] = __builtin_bswap32(cached[i]);
	return 0;
}

int SceKernelUtilsForDriver_A773A6A8(const void *plain, SceSize len, void *digest)
{
	SceSha256Context context;
	int res;

	SceKernelUtilsForDriver_D909FA2C(&context);
	res = SceKernelUtilsForDriver_236A9097(&context, plain, len);
	if (res >= 0)
		res = SceKernelUtilsForDriver_4899CD4B(&context, digest);
	return res;
}

int AesInit(SceAesCompactContext *ctx, SceSize blocksize, SceSize keysize,
	    const void *key, int mode)
{
	uint32_t keyWords, blockWords, rounds, totalWords;
	uint32_t *schedule;
	unsigned int group, index, constant = 0;
	unsigned int lastRound;

	if (ctx == NULL || key == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (((blocksize & ~0x40U) != 128 && blocksize != 256) ||
	    ((keysize & ~0x40U) != 128 && keysize != 256))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	keyWords = keysize >> 5;
	blockWords = blocksize >> 5;
	if (mode != 0)
		ctx->size = sizeof(SceAesCompactContext);
	else
		ctx->size = sizeof(SceAesFullContext);
	ctx->keyWords = keyWords;
	rounds = (blockWords >= keyWords ? blockWords : keyWords) + 6;
	ctx->rounds = rounds;
	ctx->blockWords = blockWords;
	schedule = ctx->roundKeys;
	totalWords = blockWords * (rounds + 1);
	memcpy(schedule, key, keyWords * sizeof(*schedule));
	if (keyWords <= 6) {
		for (group = keyWords; group < totalWords; group += keyWords) {
			uint32_t first = schedule[group - keyWords];
			uint32_t previous = schedule[group - 1];
			uint32_t substituted;

			substituted = SceKernelUtilsForDriver_60ED6EA9(
				(previous >> 8) | (previous << 24));
			schedule[group] = aesRoundConstants[constant++] ^ substituted ^ first;
			for (index = 1; index != keyWords && group + index < totalWords; index++)
				schedule[group + index] = schedule[group + index - 1] ^
					schedule[group + index - keyWords];
		}
	} else {
		for (group = keyWords; group < totalWords; group += keyWords) {
			uint32_t first = schedule[group - keyWords];
			uint32_t previous = schedule[group - 1];
			uint32_t substituted;

			substituted = SceKernelUtilsForDriver_60ED6EA9(
				(previous >> 8) | (previous << 24));
			schedule[group] = aesRoundConstants[constant++] ^ substituted ^ first;
			for (index = 1; index != 4 && group + index < totalWords; index++)
				schedule[group + index] = schedule[group + index - 1] ^
					schedule[group + index - keyWords];
			if (group + 4 < totalWords)
				schedule[group + 4] = SceKernelUtilsForDriver_60ED6EA9(
					schedule[group + 3]) ^ schedule[group + 4 - keyWords];
			for (index = 5; index != keyWords && group + index < totalWords; index++)
				schedule[group + index] = schedule[group + index - keyWords] ^
					schedule[group + index - 1];
		}
	}
	lastRound = totalWords - blockWords;
	if (mode == 0) {
		SceAesFullContext *full = (SceAesFullContext *)ctx;
		uint32_t *decryption = full->decryptionRoundKeys;

		for (index = 0; index != blockWords; index++)
			decryption[lastRound + index] = schedule[index];
		for (group = blockWords; group < lastRound; group += blockWords)
			for (index = 0; index != blockWords; index++)
				decryption[lastRound - group + index] =
					SceKernelUtilsForDriver_C76A7685(schedule[group + index]);
		for (index = 0; index != blockWords; index++)
			decryption[index] = schedule[lastRound + index];
	} else if (mode != 1) {
		unsigned int firstRound = blockWords;
		unsigned int finalRound = lastRound - blockWords;

		for (index = 0; index != blockWords; index++) {
			uint32_t first = schedule[index];
			uint32_t last = schedule[lastRound + index];

			schedule[index] = last;
			schedule[lastRound + index] = first;
		}
		while (firstRound < finalRound) {
			for (index = 0; index != blockWords; index++) {
				uint32_t first = SceKernelUtilsForDriver_C76A7685(
					schedule[firstRound + index]);
				uint32_t last = SceKernelUtilsForDriver_C76A7685(
					schedule[finalRound + index]);

				schedule[finalRound + index] = first;
				schedule[firstRound + index] = last;
			}
			firstRound += blockWords;
			finalRound -= blockWords;
		}
		if (firstRound == finalRound)
			for (index = 0; index != blockWords; index++)
				schedule[firstRound + index] =
					SceKernelUtilsForDriver_C76A7685(schedule[firstRound + index]);
	}
	return 0;
}

int SceKernelUtilsForDriver_F12B6451(SceAesFullContext *ctx, SceSize blocksize, SceSize keysize, const void *key)
{
	return AesInit(&ctx->encryption, blocksize, keysize, key, 0);
}

int SceKernelUtilsForDriver_EDA97D6D(SceAesCompactContext *ctx, SceSize blocksize, SceSize keysize, const void *key)
{
	return AesInit(ctx, blocksize, keysize, key, 1);
}

int SceKernelUtilsForDriver_72408E29(SceAesCompactContext *ctx, SceSize blocksize, SceSize keysize, const void *key)
{
	return AesInit(ctx, blocksize, keysize, key, 2);
}

int HmacDigest(const void *key, SceSize keyLen, const void *plain, SceSize len,
	       void *digest, const HmacAlgorithm *algorithm)
{
	union {
		SceSha1Context sha1;
		SceKernelSha2Context sha2;
	} context;
	unsigned char innerDigest[SCE_SHA256_DIGEST_SIZE];
	unsigned char keybuf[SCE_SHA256_BLOCK_SIZE];
	SceSize blockSize = algorithm->blockSize;
	SceSize digestSize = algorithm->digestSize;
	unsigned char *cursor, *end;
	int bad;

	bad = key == NULL || plain == NULL;
	if (bad)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (digest == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	memset(keybuf, 0, sizeof(keybuf));
	if (blockSize < keyLen) {
		algorithm->digest(key, keyLen, keybuf);
	} else if (keyLen != 0) {
#if FW_VERSION >= 0x04000000
		__builtin___memcpy_chk(keybuf, key, keyLen, sizeof(keybuf));
#else
		memcpy(keybuf, key, keyLen);
#endif
	}
	end = keybuf + blockSize;
	for (cursor = keybuf; cursor != end; cursor++)
		*cursor ^= 0x36;
	algorithm->init(&context);
	algorithm->update(&context, keybuf, blockSize);
	algorithm->update(&context, plain, len);
	algorithm->result(&context, innerDigest);
	for (cursor = keybuf; cursor != end; cursor++)
		*cursor ^= 0x6a;
	algorithm->init(&context);
	algorithm->update(&context, keybuf, blockSize);
	algorithm->update(&context, innerDigest, digestSize);
	algorithm->result(&context, digest);
	return 0;
}

int SceKernelUtilsForDriver_29A28957(const void *key, SceSize key_len, const void *plain, SceSize len,
		       void *digest)
{
	return HmacDigest(key, key_len, plain, len, digest, &hmacSha1Algorithm);
}

int SceKernelUtilsForDriver_7F2A7B99(const void *key, SceSize key_len, const void *plain, SceSize len,
			 void *digest)
{
	return HmacDigest(key, key_len, plain, len, digest, &hmacSha224Algorithm);
}

int SceKernelUtilsForDriver_83EFA1CC(const void *key, SceSize key_len, const void *plain, SceSize len,
			 void *digest)
{
	return HmacDigest(key, key_len, plain, len, digest, &hmacSha256Algorithm);
}




