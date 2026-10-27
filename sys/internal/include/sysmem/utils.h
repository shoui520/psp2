/*
 * SceKernelUtilsForDriver (os0:kd/sysmem.skprx): the kernel's copies of the SDK's hash, random
 * number and deflate libraries (their contexts are the SDK's), with AES, CMAC and DES; each
 * export declared once, with sysmem's definition as the prototype.
 */
#pragma once

#include <stdint.h>
#include <scetypes.h>
#include <libsha1.h>
#include <libsha224.h>
#include <libsha256.h>
#include <libmt19937.h>
#include <libdeflt.h>
#include <sysmem/aes.h>
#include <sysmem/des.h>

#ifdef __cplusplus
extern "C" {
#endif

/* an AES-CMAC computation (aes_cmac.c) */
struct CmacContext {
	SceAesCompactContext aes;
	uint64_t ullTotalLen;
	uint32_t remains;
	uint32_t computed;
	uint8_t result[32];
	uint8_t chain[32];
	uint8_t completeSubkey[32];
	uint8_t partialSubkey[32];
	uint8_t buffer[32];
};

/* Marsaglia's xorshift128 generator (xorshift128.c) */
typedef struct SceXorshift128State {
	SceUInt32 x, y, z, w;
} SceXorshift128State;

const void *sceZlibGetCompressedData(const void *src);
int SceKernelUtilsForDriver_0C39954E(struct CmacContext *ctx, SceSize blockBits, SceSize keyBits, const void *key);
int SceKernelUtilsForDriver_1DF3792A(SceSize blockBits, SceSize keyBits, const void *key, const void *plain, SceSize length, void *result);
int SceKernelUtilsForDriver_222986EF(struct CmacContext *ctx, void *result);
int sceSha256BlockUpdate(SceSha256Context *context, const void *plain, SceSize len);
int sceHmacSha1Digest(const void *key, SceSize key_len, const void *plain, SceSize len, void *digest);
/* vitasdk: ksceAesEncrypt2 */
int SceKernelUtilsForDriver_302947B6(SceAesCompactContext *ctx, const void *src, void *dst);
SceUInt32 SceKernelUtilsForDriver_335AF34D(SceXorshift128State *state);
int sceGzipDecompress(void *dst, SceSize dst_size, const void *src, uint32_t *crc32);
int sceDeflateDecompressPartial(void *dst, SceSize dst_size, const void *src, const void **next, SceDeflatePartialInputParam *cbInfo);
int sceSha1BlockUpdate(SceSha1Context *ctx, const void *plain, SceSize len);
int sceSha256BlockResult(SceSha256Context *context, void *result);
int sceSha1BlockResult(SceSha1Context *ctx, void *result);
int sceMt19937Init(SceMt19937Context *context, SceUInt32 seed);
int sceZlibGetInfo(const void *src, unsigned char *cmf, unsigned char *flg, unsigned int *dictid, const void **data);
const void *sceGzipGetCompressedData(const void *src);
/* vitasdk: ksceAesInit3 */
int SceKernelUtilsForDriver_72408E29(SceAesCompactContext *ctx, SceSize blocksize, SceSize keysize, const void *key);
int sceSha224BlockUpdate(SceSha224Context *context, const void *plain, SceSize len);
int sceHmacSha224Digest(const void *key, SceSize key_len, const void *plain, SceSize len, void *digest);
int sceHmacSha256Digest(const void *key, SceSize key_len, const void *plain, SceSize len, void *digest);
/* vitasdk: ksceMt19937GlobalUninit */
int SceKernelUtilsForDriver_875B2A1C(SceUInt32 *buffer, SceSize size);
int sceSha1Digest(const void *plain, SceSize len, void *digest);
int sceDeflateDecompress(void *dst, SceSize dst_size, const void *src, const void **next);
int sceZlibDecompress(void *dst, SceSize dst_size, const void *src, uint32_t *adler32);
SceUInt32 sceMt19937UInt(SceMt19937Context *context);
int sceSha224Digest(const void *plain, SceSize len, void *digest);
int sceSha256Digest(const void *plain, SceSize len, void *digest);
int SceKernelUtilsForDriver_B55C69B7(SceXorshift128State *state);
int SceKernelUtilsForDriver_BB6E35CD(struct CmacContext *ctx, const void *plain, SceSize length);
/* vitasdk: ksceAesEncrypt1 */
int SceKernelUtilsForDriver_C2A61770(SceAesFullContext *context, const void *src, void *dst);
int sceSha224BlockInit(SceSha224Context *context);
const char *sceGzipGetComment(const void *src);
int SceKernelUtilsForDriver_D428CC2A(SceUInt32 seed);	/* vitasdk: ksceMt19937GlobalInit */
/* vitasdk: ksceAesDecrypt1 */
int SceKernelUtilsForDriver_D8678061(SceAesFullContext *context, const void *src, void *dst);
int sceGzipIsValid(const void *src);
int sceSha256BlockInit(SceSha256Context *context);
/* vitasdk: ksceAesDecrypt2 */
int SceKernelUtilsForDriver_E39CD272(SceAesCompactContext *context, const void *src, void *dst);
int sceSha1BlockInit(SceSha1Context *ctx);
/* vitasdk: ksceAesInit2 */
int SceKernelUtilsForDriver_EDA97D6D(SceAesCompactContext *ctx, SceSize blocksize, SceSize keysize, const void *key);
/* vitasdk: ksceAesInit1 */
int SceKernelUtilsForDriver_F12B6451(SceAesFullContext *ctx, SceSize blocksize, SceSize keysize, const void *key);
const char *sceGzipGetName(const void *src);
int sceSha224BlockResult(SceSha224Context *context, void *result);
int sceGzipGetInfo(const void *src, const void **extra, const char **name, const char **comment, unsigned short *crc, const void **data);

#ifdef __cplusplus
}
#endif
