#include <libaes.h>
#include <libkernel/libkernel.h>
#include <stddef.h>
#include <stdint.h>

static const uint8_t aesRoundConstants[30] = {
	0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
	0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a, 0x2f,
	0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35, 0x6a, 0xd4,
	0xb3, 0x7d, 0xfa, 0xef, 0xc5, 0x91,
};

uint32_t _sceAesSubWord(uint32_t word) __attribute__((const));
uint32_t _sceAesInvMixColumn(uint32_t column);

int sceAesScheduleKey(SceAesContext *ctx, SceUInt32 blocksize, SceUInt32 keysize, const void *key)
{
	uint32_t keyWords, blockWords, rounds, totalWords;
	unsigned int group, index, constant = 0;
	unsigned int lastRound;

	if (ctx == NULL || key == NULL)
		return SCE_AES_ERROR_INVALID_POINTER;
	if ((blocksize & ~0x40U) != 128 && blocksize != 256)
		return SCE_AES_ERROR_INVALID_BLOCK_SIZE;
	if ((keysize & ~0x40U) != 128 && keysize != 256)
		return SCE_AES_ERROR_INVALID_KEY_SIZE;
	blockWords = blocksize >> 5;
	keyWords = keysize >> 5;
	ctx->size = sizeof(SceAesContext);
	ctx->blockWords = blockWords;
	if (blockWords >= keyWords)
		rounds = blockWords + 6;
	else
		rounds = keyWords + 6;
	ctx->keyWords = keyWords;
	ctx->rounds = rounds;
	totalWords = blockWords * (rounds + 1);
	sceClibMemcpy_safe(ctx->roundKeys, key, keyWords * sizeof(uint32_t));
	if (keyWords > 6) {
		for (group = keyWords; group < totalWords; group += keyWords) {
			uint32_t first = ctx->roundKeys[group - keyWords];
			uint32_t previous = ctx->roundKeys[group - 1];
			uint32_t substituted;

			substituted = _sceAesSubWord(
				(previous >> 8) | (previous << 24));
			ctx->roundKeys[group] = aesRoundConstants[constant++] ^ substituted ^ first;
			for (index = 1; index < 4 && group + index < totalWords; index++)
				ctx->roundKeys[group + index] = ctx->roundKeys[group + index - 1] ^
					ctx->roundKeys[group + index - keyWords];
			if (group + 4 < totalWords)
				ctx->roundKeys[group + 4] = _sceAesSubWord(
					ctx->roundKeys[group + 3]) ^ ctx->roundKeys[group + 4 - keyWords];
			for (index = 5; index < keyWords && group + index < totalWords; index++)
				ctx->roundKeys[group + index] = ctx->roundKeys[group + index - keyWords] ^
					ctx->roundKeys[group + index - 1];
		}
	} else {
		for (group = keyWords; group < totalWords; group += keyWords) {
			uint32_t first = ctx->roundKeys[group - keyWords];
			uint32_t previous = ctx->roundKeys[group - 1];
			uint32_t substituted;

			substituted = _sceAesSubWord(
				(previous >> 8) | (previous << 24));
			ctx->roundKeys[group] = aesRoundConstants[constant++] ^ substituted ^ first;
			for (index = 1; index < keyWords && group + index < totalWords; index++)
				ctx->roundKeys[group + index] = ctx->roundKeys[group + index - 1] ^
					ctx->roundKeys[group + index - keyWords];
		}
	}
	for (index = 0; index < blockWords; index++)
		ctx->decryptionRoundKeys[totalWords - blockWords + index] = ctx->roundKeys[index];
	for (group = blockWords; group < totalWords - blockWords; group += blockWords)
		for (index = 0; index < blockWords; index++)
			ctx->decryptionRoundKeys[totalWords - blockWords - group + index] =
				_sceAesInvMixColumn(ctx->roundKeys[group + index]);
	for (index = totalWords - blockWords; index < totalWords; index++)
		ctx->decryptionRoundKeys[index - totalWords + blockWords] = ctx->roundKeys[index];
	return 0;
}
