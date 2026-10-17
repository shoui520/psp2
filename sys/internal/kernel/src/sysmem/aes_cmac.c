/* KernelUtils CMAC over the variable-block-size Rijndael interface.
 * The encryption backend's verification remains incomplete.
 */

#include <sysmem/utils.h>
#include <kernel/error.h>
#include <sysmem/aes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <fw.h>

_Static_assert(sizeof(struct CmacContext) == 0x2a0, "CMAC context size");
_Static_assert(offsetof(struct CmacContext, ullTotalLen) == 0x1f0, "CMAC length offset");
_Static_assert(offsetof(struct CmacContext, remains) == 0x1f8, "CMAC buffered length offset");
_Static_assert(offsetof(struct CmacContext, result) == 0x200, "CMAC result offset");
_Static_assert(offsetof(struct CmacContext, buffer) == 0x280, "CMAC input buffer offset");

void *__memcpy_chk(void *dst, const void *src, size_t size, size_t capacity);

/* Only the debug original fortifies the stack block copies. */
#if FW_SINCE(0x04000000)
#define CmacCopyBlock(dst, src, size) __memcpy_chk(dst, src, size, 32)
#else
#define CmacCopyBlock(dst, src, size) memcpy(dst, src, size)
#endif

int SceKernelUtilsForDriver_0C39954E(struct CmacContext *ctx,
				  SceSize blockBits, SceSize keyBits,
				  const void *key)
{
	uint8_t encryptedZero[32];
	uint32_t index, last;
	int ret;

	if (key == NULL || ctx == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	memset(ctx, 0, sizeof(*ctx));
	ret = SceKernelUtilsForDriver_EDA97D6D((SceAesContext *)&ctx->aes, blockBits, keyBits, key);
	if (ret < 0)
		return ret;
	SceKernelUtilsForDriver_302947B6((SceAesContext *)&ctx->aes, ctx->chain, encryptedZero);
	last = ctx->aes.blockWords * 4 - 1;
	index = 0;
	do {
		ctx->completeSubkey[index] = (encryptedZero[index] << 1) |
					    (encryptedZero[index + 1] >> 7);
		index++;
	} while (index < last);
	ctx->completeSubkey[last] = encryptedZero[last] << 1;
	if (encryptedZero[0] & 0x80)
		ctx->completeSubkey[last] ^= 0x87;
	index = 0;
	do {
		ctx->partialSubkey[index] = (ctx->completeSubkey[index] << 1) |
					   (ctx->completeSubkey[index + 1] >> 7);
		index++;
	} while (index < last);
	ctx->partialSubkey[last] = ctx->completeSubkey[last] << 1;
	if (ctx->completeSubkey[0] & 0x80)
		ctx->partialSubkey[last] ^= 0x87;
	return 0;
}

int SceKernelUtilsForDriver_BB6E35CD(struct CmacContext *ctx,
				  const void *plain, SceSize length)
{
	const uint8_t *input = plain;
	uint8_t block[32];
	uint32_t blockBytes, pending, index, consumed;

	if (plain == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (ctx == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	blockBytes = ctx->aes.blockWords * 4;
	pending = ctx->remains + length;
	ctx->ullTotalLen += length;
	if (pending > blockBytes) {
		pending -= blockBytes;
		for (;;) {
			if (ctx->remains != 0) {
				consumed = blockBytes - ctx->remains;
				CmacCopyBlock(block, ctx->buffer, ctx->remains);
				memcpy(block + ctx->remains, input, consumed);
				input += consumed;
				ctx->remains = 0;
			} else {
				CmacCopyBlock(block, input, blockBytes);
				input += blockBytes;
			}
			for (index = 0; index < blockBytes; index++)
				ctx->chain[index] ^= block[index];
			SceKernelUtilsForDriver_302947B6((SceAesContext *)&ctx->aes, ctx->chain, ctx->chain);
			if (pending <= blockBytes)
				break;
			pending -= blockBytes;
		}
	}
	if (pending != 0)
		memcpy(ctx->buffer + ctx->remains, input, pending - ctx->remains);
	ctx->remains = pending;
	ctx->computed = 0;
	return 0;
}

int SceKernelUtilsForDriver_222986EF(struct CmacContext *ctx, void *result)
{
	uint32_t blockBytes, index;

	if (result == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (ctx == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	blockBytes = ctx->aes.blockWords * 4;
	if (ctx->computed == 0) {
		if (ctx->ullTotalLen == 0) {
			memcpy(ctx->chain, ctx->partialSubkey, blockBytes);
			ctx->chain[0] ^= 0x80;
		} else if (ctx->remains == blockBytes) {
			for (index = 0; index < blockBytes; index++)
				ctx->chain[index] ^= ctx->buffer[index] ^
						     ctx->completeSubkey[index];
		} else {
			for (index = 0; index < ctx->remains; index++)
				ctx->chain[index] ^= ctx->buffer[index];
			for (index = 0; index < blockBytes; index++)
				ctx->chain[index] ^= ctx->partialSubkey[index];
			ctx->chain[ctx->remains] ^= 0x80;
		}
		SceKernelUtilsForDriver_302947B6((SceAesContext *)&ctx->aes, ctx->chain, ctx->result);
		ctx->computed = 1;
	}
	memcpy(result, ctx->result, blockBytes);
	return 0;
}

int SceKernelUtilsForDriver_1DF3792A(SceSize blockBits, SceSize keyBits,
				  const void *key, const void *plain,
				  SceSize length, void *result)
{
	struct CmacContext ctx;
	int ret;

	ret = SceKernelUtilsForDriver_0C39954E(&ctx, blockBits, keyBits, key);
	if (ret < 0)
		return ret;
	ret = SceKernelUtilsForDriver_BB6E35CD(&ctx, plain, length);
	if (ret < 0)
		return ret;
	ret = SceKernelUtilsForDriver_222986EF(&ctx, result);
	return ret & (ret >> 31);
}
