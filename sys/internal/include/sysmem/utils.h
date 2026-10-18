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

/* vitasdk: ksceZlibGetCompressedData */
const void *SceKernelUtilsForDriver_01EB6C45(const void *src);
int SceKernelUtilsForDriver_0C39954E(struct CmacContext *ctx, SceSize blockBits, SceSize keyBits, const void *key);
int SceKernelUtilsForDriver_1DF3792A(SceSize blockBits, SceSize keyBits, const void *key, const void *plain, SceSize length, void *result);
int SceKernelUtilsForDriver_222986EF(struct CmacContext *ctx, void *result);
/* vitasdk: ksceSha256BlockUpdate */
int SceKernelUtilsForDriver_236A9097(SceSha256Context *context, const void *plain, SceSize len);
/* vitasdk: ksceHmacSha1Digest */
int SceKernelUtilsForDriver_29A28957(const void *key, SceSize key_len, const void *plain, SceSize len, void *digest);
/* vitasdk: ksceAesEncrypt2 */
int SceKernelUtilsForDriver_302947B6(SceAesCompactContext *ctx, const void *src, void *dst);
SceUInt32 SceKernelUtilsForDriver_335AF34D(SceXorshift128State *state);
/* vitasdk: ksceGzipDecompress */
int SceKernelUtilsForDriver_367EE3DF(void *dst, SceSize dst_size, const void *src, uint32_t *crc32);
/* vitasdk: ksceDeflateDecompressPartial */
int SceKernelUtilsForDriver_3D74CCDF(void *dst, SceSize dst_size, const void *src, const void **next, SceDeflatePartialInputParam *cbInfo);
/* vitasdk: ksceSha1BlockUpdate */
int SceKernelUtilsForDriver_478A6F3C(SceSha1Context *ctx, const void *plain, SceSize len);
/* vitasdk: ksceSha256BlockResult */
int SceKernelUtilsForDriver_4899CD4B(SceSha256Context *context, void *result);
/* vitasdk: ksceSha1BlockResult */
int SceKernelUtilsForDriver_48F24106(SceSha1Context *ctx, void *result);
/* vitasdk: ksceMt19937Init */
int SceKernelUtilsForDriver_4C9A5730(SceMt19937Context *context, SceUInt32 seed);
/* vitasdk: ksceZlibGetInfo */
int SceKernelUtilsForDriver_5B9BCD75(const void *src, unsigned char *cmf, unsigned char *flg, unsigned int *dictid, const void **data);
/* vitasdk: ksceGzipGetCompressedData */
const void *SceKernelUtilsForDriver_63619124(const void *src);
/* vitasdk: ksceAesInit3 */
int SceKernelUtilsForDriver_72408E29(SceAesCompactContext *ctx, SceSize blocksize, SceSize keysize, const void *key);
/* vitasdk: ksceSha224BlockUpdate */
int SceKernelUtilsForDriver_7556E611(SceSha224Context *context, const void *plain, SceSize len);
/* vitasdk: ksceHmacSha224Digest */
int SceKernelUtilsForDriver_7F2A7B99(const void *key, SceSize key_len, const void *plain, SceSize len, void *digest);
/* vitasdk: ksceHmacSha256Digest */
int SceKernelUtilsForDriver_83EFA1CC(const void *key, SceSize key_len, const void *plain, SceSize len, void *digest);
/* vitasdk: ksceMt19937GlobalUninit */
int SceKernelUtilsForDriver_875B2A1C(SceUInt32 *buffer, SceSize size);
/* vitasdk: ksceSha1Digest */
int SceKernelUtilsForDriver_87DC7F2F(const void *plain, SceSize len, void *digest);
/* vitasdk: ksceDeflateDecompress */
int SceKernelUtilsForDriver_8AF1FAD4(void *dst, SceSize dst_size, const void *src, const void **next);
/* vitasdk: ksceZlibDecompress */
int SceKernelUtilsForDriver_900148DB(void *dst, SceSize dst_size, const void *src, uint32_t *adler32);
/* vitasdk: ksceMt19937UInt */
SceUInt32 SceKernelUtilsForDriver_92AEDFBC(SceMt19937Context *context);
/* vitasdk: ksceSha224Digest */
int SceKernelUtilsForDriver_9EA9D4DC(const void *plain, SceSize len, void *digest);
/* vitasdk: ksceSha256Digest */
int SceKernelUtilsForDriver_A773A6A8(const void *plain, SceSize len, void *digest);
int SceKernelUtilsForDriver_B55C69B7(SceXorshift128State *state);
int SceKernelUtilsForDriver_BB6E35CD(struct CmacContext *ctx, const void *plain, SceSize length);
/* vitasdk: ksceAesEncrypt1 */
int SceKernelUtilsForDriver_C2A61770(SceAesFullContext *context, const void *src, void *dst);
/* vitasdk: ksceSha224BlockInit */
int SceKernelUtilsForDriver_C762EA6D(SceSha224Context *context);
const char *SceKernelUtilsForDriver_CD3AE08F(const void *src);	/* vitasdk: ksceGzipGetComment */
int SceKernelUtilsForDriver_D428CC2A(SceUInt32 seed);	/* vitasdk: ksceMt19937GlobalInit */
/* vitasdk: ksceAesDecrypt1 */
int SceKernelUtilsForDriver_D8678061(SceAesFullContext *context, const void *src, void *dst);
int SceKernelUtilsForDriver_D8FAEFD4(const void *src);	/* vitasdk: ksceGzipIsValid */
/* vitasdk: ksceSha256BlockInit */
int SceKernelUtilsForDriver_D909FA2C(SceSha256Context *context);
/* vitasdk: ksceAesDecrypt2 */
int SceKernelUtilsForDriver_E39CD272(SceAesCompactContext *context, const void *src, void *dst);
int SceKernelUtilsForDriver_E4390FFA(SceSha1Context *ctx);	/* vitasdk: ksceSha1BlockInit */
/* vitasdk: ksceAesInit2 */
int SceKernelUtilsForDriver_EDA97D6D(SceAesCompactContext *ctx, SceSize blocksize, SceSize keysize, const void *key);
/* vitasdk: ksceAesInit1 */
int SceKernelUtilsForDriver_F12B6451(SceAesFullContext *ctx, SceSize blocksize, SceSize keysize, const void *key);
const char *SceKernelUtilsForDriver_F901FD3E(const void *src);	/* vitasdk: ksceGzipGetName */
/* vitasdk: ksceSha224BlockResult */
int SceKernelUtilsForDriver_FBF2A442(SceSha224Context *context, void *result);
/* vitasdk: ksceGzipGetInfo */
int SceKernelUtilsForDriver_FFC6A10F(const void *src, const void **extra, const char **name, const char **comment, unsigned short *crc, const void **data);

#ifdef __cplusplus
}
#endif
